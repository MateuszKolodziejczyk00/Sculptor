#include "SculptorShader.hlsli"
#include "Utils/SceneViewUtils.hlsli"


[[descriptor_set(RenderViewDS, 0)]]
[[descriptor_set(ShadowsBilateralBlurDS, 1)]]


struct CS_INPUT
{
    uint3 globalID : SV_DispatchThreadID;
    uint3 groupID : SV_GroupID;
    uint3 localID : SV_GroupThreadID;
};


#define KERNEL_SIZE 6

#define GROUP_SIZE 128

#ifndef IS_HORIZONTAL
#error "IS_HORIZONTAL not defined"
#endif // IS_HORIZONTAL


#define SHARED_DATA_SIZE (GROUP_SIZE + 2 * KERNEL_SIZE)

groupshared float3 sharedSampleData[SHARED_DATA_SIZE]; // x - (x or y view space depending on IS_HORIZONTAL), y - depth in view space, z - shadow


static const float maxDistanceToBlurredPlane = 0.1f;


void BlurPlane(in int baseSampleIdx, in float3 baseSampleData, in int direction, inout float shadow, inout float samplesNum)
{
    const float3 sample1 = sharedSampleData[baseSampleIdx + direction * 1];
    const float3 sample2 = sharedSampleData[baseSampleIdx + direction * 2];

    const float dPos = abs(sample1.x - sample2.x);
    
    const float maxDistToPlane = 0.15f;
    // sample2 here gives much better results than sample1
    const float distToPlane = abs(sample2.y - baseSampleData.y);

    if (dPos != 0.f && distToPlane <= maxDistToPlane)
    {
        const float dDepth = abs(sample1.y - sample2.y) / dPos;

        const float baseSampleDepthOnPlane = (baseSampleData.x - sample1.x) * dDepth + sample1.y;

        if(abs(baseSampleData.y - baseSampleDepthOnPlane) <= maxDistanceToBlurredPlane)
        {
            shadow += (sample1.z + sample2.z);
            samplesNum += 2.f;

            const int kernelSize = lerp(2, KERNEL_SIZE, saturate(0.2f / abs(dPos)));

            for (int sampleOffset = 3; sampleOffset <= kernelSize; sampleOffset += 1)
            {
                const int sampleIdx = baseSampleIdx + sampleOffset * direction;
                const float3 sampleData = sharedSampleData[sampleIdx];
                const float baseSampleDepthOnPlaneForCurrentSample = (baseSampleData.x - sampleData.x) * dDepth + sampleData.y;
                if (abs(baseSampleDepthOnPlaneForCurrentSample - baseSampleData.y) <= maxDistanceToBlurredPlane)
                {
                    shadow += sampleData.z;
                    samplesNum += 1.f;
                }
                else
                {
                    break;
                }
            }
        }
    }
}


[numthreads(GROUP_SIZE, 1, 1)]
void ShadowsBilateralBlurCS(CS_INPUT input)
{
    const uint2 groupSize = (IS_HORIZONTAL ? uint2(GROUP_SIZE, 1) : uint2(1, GROUP_SIZE));
    const uint2 localID = IS_HORIZONTAL ? input.localID.xy : input.localID.yx;
    const uint2 groupID = IS_HORIZONTAL ? input.groupID.xy : input.groupID.yx;

    const int2 groupBeginPixel = groupID * groupSize;
    
    const int2 pixel = groupBeginPixel + localID;

    uint2 outputRes;
    u_outputTexture.GetDimensions(outputRes.x, outputRes.y);

    const float2 invRes = rcp(float2(outputRes));

    for (int i = input.localID.x; i < SHARED_DATA_SIZE; i += GROUP_SIZE)
    {
        const int2 offset = (IS_HORIZONTAL ? int2(i - KERNEL_SIZE, 0) : int2(0, i - KERNEL_SIZE));
        
        const float2 sampleUV = (groupBeginPixel + offset + 0.5f) * invRes;
        
        const float depth = u_depth.SampleLevel(u_depthSampler, sampleUV, 0);
        const float3 viewSpace = NDCToViewSpace(float3(sampleUV * 2.f - 1.f, depth), u_sceneView.inverseProjection);

        const float2 locationAndDepth = IS_HORIZONTAL ? viewSpace.yx : viewSpace.zx;

        const float sampleShadow = u_inputTexture.SampleLevel(u_inputSampler, sampleUV, 0);

        sharedSampleData[i] = float3(locationAndDepth, sampleShadow);
    }
    
    GroupMemoryBarrierWithGroupSync();

    if(pixel.x < outputRes.x && pixel.y < outputRes.y)
    {
        const int baseSampleIdx = input.localID.x + KERNEL_SIZE;
        const float3 baseSampleData = sharedSampleData[baseSampleIdx];

        float shadow = baseSampleData.z;
        float samplesNum = 1.f;

        // Left side
        BlurPlane(baseSampleIdx, baseSampleData, 1, shadow, samplesNum);

        // Right side
        BlurPlane(baseSampleIdx, baseSampleData, -1, shadow, samplesNum);

        u_outputTexture[pixel] = (shadow / samplesNum);
    }
}