#include "SculptorShader.hlsli"
#include "Utils/SceneViewUtils.hlsli"

[[descriptor_set(DownsampleGeometryTexturesDS, 0)]]
[[descriptor_set(RenderViewDS, 1)]]


struct CS_INPUT
{
    uint3 globalID : SV_DispatchThreadID;
};


static const float depthDiffThreshold = 0.04f;


float ComputeSampleWeight(float sampleDepthDiff)
{
    return sampleDepthDiff <= depthDiffThreshold ? rcp(Pow2(sampleDepthDiff / depthDiffThreshold + 1.f)) : 0.f;
}


[numthreads(8, 8, 1)]
void DownsampleGeometryTexturesCS(CS_INPUT input)
{
    const uint2 pixel = input.globalID.xy;
    
    uint2 outputRes;
    u_depthTextureHalfRes.GetDimensions(outputRes.x, outputRes.y);

    if(pixel.x < outputRes.x && pixel.y < outputRes.y)
    {
        uint2 inputRes;
        u_depthTexture.GetDimensions(inputRes.x, inputRes.y);

        const float2 pixelSize = rcp(float2(inputRes));

        const float2 uv = (float2(2 * pixel) + 0.5f) * pixelSize;
        const float2 uv0 = uv + float2(0.f, 1.f) * pixelSize;
        const float2 uv1 = uv + float2(1.f, 1.f) * pixelSize;
        const float2 uv2 = uv + float2(1.f, 0.f) * pixelSize;
        const float2 uv3 = uv + float2(0.f, 0.f) * pixelSize;

        const float4 depths = u_depthTexture.Gather(u_nearestSampler, uv, 0);

        const float2 motion0 = u_motionTexture.SampleLevel(u_nearestSampler, uv0, 0);
        const float2 motion1 = u_motionTexture.SampleLevel(u_nearestSampler, uv1, 0);
        const float2 motion2 = u_motionTexture.SampleLevel(u_nearestSampler, uv2, 0);
        const float2 motion3 = u_motionTexture.SampleLevel(u_nearestSampler, uv3, 0);

        const float3 normal0 = u_geometryNormalsTexture.SampleLevel(u_nearestSampler, uv0, 0) * 2.f - 1.f;
        const float3 normal1 = u_geometryNormalsTexture.SampleLevel(u_nearestSampler, uv1, 0) * 2.f - 1.f;
        const float3 normal2 = u_geometryNormalsTexture.SampleLevel(u_nearestSampler, uv2, 0) * 2.f - 1.f;
        const float3 normal3 = u_geometryNormalsTexture.SampleLevel(u_nearestSampler, uv3, 0) * 2.f - 1.f;

        const float4 linearDepths = ComputeLinearDepth(depths, u_sceneView);

        const float minLinearDepth = min(min(min(linearDepths.x, linearDepths.y), linearDepths.z), linearDepths.w);
        const float4 sampleDepthDiff = abs(linearDepths - minLinearDepth);

        const float weight0 = ComputeSampleWeight(sampleDepthDiff.x);
        const float weight1 = ComputeSampleWeight(sampleDepthDiff.y);
        const float weight2 = ComputeSampleWeight(sampleDepthDiff.z);
        const float weight3 = ComputeSampleWeight(sampleDepthDiff.w);

        const float rcpWeightSum = rcp(weight0 + weight1 + weight2 + weight3);

        const float2 motion = (motion0 * weight0 + motion1 * weight1 + motion2 * weight2 + motion3 * weight3) * rcpWeightSum;
        const float3 normal = (normal0 * weight0 + normal1 * weight1 + normal2 * weight2 + normal3 * weight3) * rcpWeightSum;

        const float closestDepth = max(max(max(depths.x, depths.y), depths.z), depths.w);
        u_depthTextureHalfRes[pixel]           = closestDepth;
        u_motionTextureHalfRes[pixel]          = motion;
        u_geometryNormalsTextureHalfRes[pixel] = normal * 0.5f + 0.5f;
    }
}
