#include "SculptorShader.hlsli"
#include "Utils/SceneViewUtils.hlsli"

[[descriptor_set(SpatialATrousFilterDS, 0)]]
[[descriptor_set(RenderViewDS, 1)]]


struct CS_INPUT
{
    uint3 globalID : SV_DispatchThreadID;
};


float NormalWeight(float3 centerNormal, float3 sampleNomral)
{
    return pow(max(0.f, dot(centerNormal, sampleNomral)), 128);
}


float WorldLocationWeight(float3 centerWS, float3 sampleWS)
{
    const float3 offset = sampleWS - centerWS;
    if (dot(offset, offset) > Pow2(0.1f))
    {
        return 0.f;
    }
    return 1.0f / Pow4(1.0f + distance(centerWS, sampleWS));
}


[numthreads(8, 8, 1)]
void SpatialATrousFilterCS(CS_INPUT input)
{
    const uint2 pixel = input.globalID.xy;
    
    uint2 outputRes;
    u_outputTexture.GetDimensions(outputRes.x, outputRes.y);

    if(pixel.x < outputRes.x && pixel.y < outputRes.y)
    {
        const float2 pixelSize = rcp(float2(outputRes));
        const float2 uv = (float2(pixel) + 0.5f) * pixelSize;

        [branch]
        if(u_params.hasValidVarianceTexture)
        {
            const float variance = u_varianceTexture.SampleLevel(u_nearestSampler, uv, 0.0f).x;
            if(variance < 0.02f)
            {
                u_outputTexture[pixel] = u_inputTexture.SampleLevel(u_nearestSampler, uv, 0.f);
                return;
            }
        }

        const float3 normal = u_geometryNormalsTexture.SampleLevel(u_nearestSampler, uv, 0.0f).xyz * 2.f - 1.f;

        const float depth = u_depthTexture.SampleLevel(u_nearestSampler, uv, 0.0f).x;
        const float3 ndc = float3(uv * 2.f - 1.f, depth);
        const float3 centerWS = NDCToWorldSpace(ndc, u_sceneView);

        const float kernel[3] = { 3.f / 8.f, 1.f / 4.f, 1.f / 16.f };

        float4 valueSum = 0.0f;
        float weightSum = 0.0f;
        for (int x = -2; x <= 2; ++x)
        {
            [unroll]
            for (int y = -2; y <= 2; ++y)
            {
                const int k = max(abs(x), abs(y));
                float w = kernel[k];

                const float2 sampleUV = uv + float2(x, y) * u_params.samplesOffset * pixelSize;
                const float3 sampleNormal = u_geometryNormalsTexture.SampleLevel(u_nearestSampler, sampleUV, 0.0f).xyz * 2.f - 1.f;

                const float sampleDepth = u_depthTexture.SampleLevel(u_nearestSampler, sampleUV, 0.0f).x;
                const float3 sampleNDC = float3(sampleUV * 2.f - 1.f, sampleDepth);
                const float3 sampleWS = NDCToWorldSpace(sampleNDC, u_sceneView);

                const float wn = NormalWeight(normal, sampleNormal);
                const float wl = WorldLocationWeight(centerWS, sampleWS);
                
                const float weight = w * wn * wl;

                valueSum += u_inputTexture.SampleLevel(u_nearestSampler, sampleUV, 0.f) * weight;
                weightSum += weight;
            }
        }

        const float4 output = valueSum / weightSum;
        u_outputTexture[pixel] = output;
    }
}
