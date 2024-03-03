#include "SculptorShader.hlsli"

[[descriptor_set(SRATrousFilterDS, 0)]]
[[descriptor_set(RenderViewDS, 1)]]

#include "Utils/SceneViewUtils.hlsli"
#include "SpecularReflections/SpecularReflectionsCommon.hlsli"


struct CS_INPUT
{
    uint3 globalID : SV_DispatchThreadID;
};


float NormalWeight(in float3 centerNormal, in float3 sampleNormal)
{
    return saturate(pow(max(0.f, dot(centerNormal, sampleNormal)), 128.f));
}


float WorldLocationWeight(in float3 centerWS, in float3 sampleWS)
{
    const float3 diff = centerWS - sampleWS;
    const float dist2 = dot(diff, diff);
    return saturate(1.f - sqrt(dist2) / 0.015f);
}


[numthreads(8, 8, 1)]
void SRATrousFilterCS(CS_INPUT input)
{
    const int2 pixel = input.globalID.xy;
    
    uint2 outputRes;
    u_outputTexture.GetDimensions(outputRes.x, outputRes.y);

    if(pixel.x < outputRes.x && pixel.y < outputRes.y)
    {
        const float2 pixelSize = rcp(float2(outputRes));
        const float2 uv = (float2(pixel) + 0.5f) * pixelSize;

        const float3 normal = u_normalsTexture.Load(uint3(pixel, 0)).xyz * 2.f - 1.f;

        const float kernel[3] = { 3.f / 8.f, 1.f / 4.f, 1.f / 16.f };

        const float centerDepth = ComputeLinearDepth(u_depthTexture.Load(uint3(pixel, 0)), u_sceneView);

        if(centerDepth < 0.000001f)
        {
            return;
        }

        const float roughness = u_specularColorRoughnessTexture.Load(uint3(pixel, 0)).w;

        if(roughness <= SPECULAR_TRACE_MAX_ROUGHNESS || roughness > GLOSSY_TRACE_MAX_ROUGHNESS)
        {
            u_outputTexture[pixel] = u_inputTexture.Load(uint3(pixel, 0));
            return;
        }
        
        const float lumStdDevMultiplier = lerp(4.f, 15.f, roughness);
        
        const float3 centerWS = NDCToWorldSpaceNoJitter(float3(uv * 2.f - 1.f, centerDepth), u_sceneView);
        float centerLuminanceStdDev = 0.f;
        float stdDevWeightSum = 0.f;

        for (int y = -1; y <= 1; ++y)
        {
            [unroll]
            for (int x = -1; x <= 1; ++x)
            {
                const float w = kernel[abs(x)] * kernel[abs(y)];

                const int2 samplePixel = clamp(pixel + int2(x, y) * u_params.samplesOffset, int2(0, 0), int2(outputRes));
                centerLuminanceStdDev += u_luminanceStdDevTexture[samplePixel] * w;

                stdDevWeightSum += w;
            }
        }
        centerLuminanceStdDev /= stdDevWeightSum;

        const float lumCenter = Luminance(u_inputTexture.Load(uint3(pixel, 0)));

        float3 luminanceSum = 0.0f;
        
        float lumSum = 0.0f;
        float lumSquaredSum = 0.0f;

        float weightSum = 0.000001f;

        for (int y = -2; y <= 2; ++y)
        {
            [unroll]
            for (int x = -2; x <= 2; ++x)
            {
                const int2 samplePixel = clamp(pixel + int2(x, y) * u_params.samplesOffset, int2(0, 0), int2(outputRes));
                const float w = kernel[abs(x)] * kernel[abs(y)];

                const float3 sampleNormal = u_normalsTexture.Load(uint3(samplePixel, 0)).xyz * 2.f - 1.f;

                const float sampleDepth = ComputeLinearDepth(u_depthTexture.Load(uint3(samplePixel, 0)), u_sceneView);

                if(sampleDepth < 0.000001f)
                {
                    continue;
                }

                const float3 sampleWS = NDCToWorldSpaceNoJitter(float3((uv + float2(x, y) * rcp(float2(outputRes))) * 2.f - 1.f, sampleDepth), u_sceneView);
                const float3 luminance = u_inputTexture.Load(uint3(samplePixel, 0));
                const float lum = Luminance(luminance);

                const float wn = max(NormalWeight(normal, sampleNormal), 0.f);
                const float dw = WorldLocationWeight(centerWS, sampleWS);
                const float lw = min(abs(lumCenter - lum) / (centerLuminanceStdDev * lumStdDevMultiplier + 0.001f), 5.f);
                
                const float weight = exp(-lw) * wn * dw * w;

                luminanceSum += luminance * weight;

                const float lumW = lum * weight;

                lumSum += lumW;
                lumSquaredSum += lumW * lumW;

                weightSum += weight;
            }
        }

        const float3 outLuminance = luminanceSum / weightSum;
        u_outputTexture[pixel] = outLuminance;

        
        lumSum /= weightSum;
        lumSquaredSum /= weightSum;

        const float variance = abs(lumSquaredSum - lumSum * lumSum);
        const float stdDev = sqrt(variance);

        u_luminanceStdDevTexture[pixel] = stdDev;
    }
}
