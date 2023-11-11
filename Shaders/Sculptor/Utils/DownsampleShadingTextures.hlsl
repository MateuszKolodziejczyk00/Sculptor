#include "SculptorShader.hlsli"
#include "Utils/SceneViewUtils.hlsli"

[[descriptor_set(DownsampleShadingTexturesDS, 0)]]
[[descriptor_set(RenderViewDS, 1)]]


struct CS_INPUT
{
    uint3 globalID : SV_DispatchThreadID;
};


static const float depthDiffThreshold = 0.0001f;


float ComputeSampleWeight(float sampleDepthDiff)
{
    return sampleDepthDiff <= depthDiffThreshold ? rcp(Pow2(sampleDepthDiff / depthDiffThreshold + 1.f)) : 0.f;
}


[numthreads(8, 8, 1)]
void DownsampleShadingTexturesCS(CS_INPUT input)
{
    const uint2 pixel = input.globalID.xy;
    
    uint2 outputRes;
    u_shadingNormalsTextureHalfRes.GetDimensions(outputRes.x, outputRes.y);

    if(pixel.x < outputRes.x && pixel.y < outputRes.y)
    {
        uint2 inputRes;
        u_depthTexture.GetDimensions(inputRes.x, inputRes.y);
        
        const float2 inputPixelSize = rcp(float2(inputRes));
        const float2 uv = (float2(2 * pixel) + 0.5f) * inputPixelSize;

        const uint3 inputPixel0 = uint3(clamp(pixel * 2 + uint2(0, 1), 0, inputRes - 1u), 0u);
        const uint3 inputPixel1 = uint3(clamp(pixel * 2 + uint2(1, 1), 0, inputRes - 1u), 0u);
        const uint3 inputPixel2 = uint3(clamp(pixel * 2 + uint2(1, 0), 0, inputRes - 1u), 0u);
        const uint3 inputPixel3 = uint3(clamp(pixel * 2 + uint2(0, 0), 0, inputRes - 1u), 0u);

        const float4 depths = u_depthTexture.Gather(u_nearestSampler, uv, 0);

        const float4 specularRoughness0 = u_specularColorRoughnessTexture.Load(inputPixel0);
        const float4 specularRoughness1 = u_specularColorRoughnessTexture.Load(inputPixel1);
        const float4 specularRoughness2 = u_specularColorRoughnessTexture.Load(inputPixel2);
        const float4 specularRoughness3 = u_specularColorRoughnessTexture.Load(inputPixel3);

        const float3 normal0 = u_shadingNormalsTexture.Load(inputPixel0) * 2.f - 1.f;
        const float3 normal1 = u_shadingNormalsTexture.Load(inputPixel1) * 2.f - 1.f;
        const float3 normal2 = u_shadingNormalsTexture.Load(inputPixel2) * 2.f - 1.f;
        const float3 normal3 = u_shadingNormalsTexture.Load(inputPixel3) * 2.f - 1.f;

        const float4 linearDepths = ComputeLinearDepth(depths, u_sceneView);

        const float minLinearDepth = min(min(min(linearDepths.x, linearDepths.y), linearDepths.z), linearDepths.w);
        const float4 sampleDepthDiff = abs(linearDepths - minLinearDepth);

        const float weight0 = ComputeSampleWeight(sampleDepthDiff.x);
        const float weight1 = ComputeSampleWeight(sampleDepthDiff.y);
        const float weight2 = ComputeSampleWeight(sampleDepthDiff.z);
        const float weight3 = ComputeSampleWeight(sampleDepthDiff.w);

        const float rcpWeightSum = rcp(weight0 + weight1 + weight2 + weight3);

        const float4 specularRoughness = (specularRoughness0 * weight0 + specularRoughness1 * weight1 + specularRoughness2 * weight2 + specularRoughness3 * weight3) * rcpWeightSum;
        const float3 normal = normalize((normal0 * weight0 + normal1 * weight1 + normal2 * weight2 + normal3 * weight3) * rcpWeightSum);

        u_specularColorRoughnessTextureHalfRes[pixel] = specularRoughness;
        u_shadingNormalsTextureHalfRes[pixel]         = normal * 0.5f + 0.5f;
    }
}
