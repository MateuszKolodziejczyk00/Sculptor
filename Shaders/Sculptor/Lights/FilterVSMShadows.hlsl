#include "SculptorShader.hlsli"

[[descriptor_set(FilterVSMDS, 0)]]

#include "Utils/SceneViewUtils.hlsli"


#ifndef FILTER_DEPTH
#error "FILTER_DEPTH is not defined"
#endif // FILTER_DEPTH


#define GROUP_SIZE 256
#define HALF_KERNEL 7
#define SHARED_DATA_ELEMENTS_NUM (GROUP_SIZE + HALF_KERNEL * 2)


struct CS_INPUT
{
    uint3 globalID : SV_DispatchThreadID;
    uint3 groupID : SV_GroupID;
    uint3 localID : SV_GroupThreadID;
};


static const float kernel[2 * HALF_KERNEL + 1] = { 0.0005, 0.0024, 0.0092, 0.0278, 0.0656, 0.1210, 0.1747, 0.1974, 0.1747, 0.1210, 0.0656, 0.0278, 0.0092, 0.0024, 0.0005 };

groupshared float2 g_grupInput[SHARED_DATA_ELEMENTS_NUM];

#define IS_HORIZONTAL (FILTER_DEPTH)


#if IS_HORIZONTAL

#define GROUP_SIZE_X GROUP_SIZE
#define GROUP_SIZE_Y 1

#else // IS_HORIZONTAL

#define GROUP_SIZE_X 1
#define GROUP_SIZE_Y GROUP_SIZE

#endif // IS_HORIZONTAL


[numthreads(GROUP_SIZE_X, GROUP_SIZE_Y, 1)]
void FilterVSMShadowMapCS(CS_INPUT input)
{
    int2 outputRes;
    u_output.GetDimensions(outputRes.x, outputRes.y);

    const int2 pixelsDirection = IS_HORIZONTAL ? int2(1, 0) : int2(0, 1);

    const int2 groupBeginPixel = input.groupID.xy * int2(GROUP_SIZE_X, GROUP_SIZE_Y);

    const int localIdx = IS_HORIZONTAL ? input.localID.x : input.localID.y;
    
    for (int i = localIdx; i < SHARED_DATA_ELEMENTS_NUM; i += GROUP_SIZE)
    {
        const int2 offset = (i - HALF_KERNEL) * pixelsDirection;
        const int2 coords = clamp(groupBeginPixel + offset, 0, outputRes - 1);

 #if FILTER_DEPTH

        const float depth = u_depth.Load(uint3(coords, 0));

        g_grupInput[i] = float2(depth, depth * depth);

#else // FILTER_DEPTH

        g_grupInput[i] = u_moments.Load(uint3(coords, 0));

#endif // IS_HORIZONTAL
    }

    GroupMemoryBarrierWithGroupSync();
   
    const int2 pixel = input.globalID.xy;

    if(pixel.x < outputRes.x && pixel.y < outputRes.y)
    {
        float2 blurredMoments = 0.f;
        
        for (int i = 0; i <= HALF_KERNEL * 2; ++i)
        {
            blurredMoments += g_grupInput[localIdx + i] * kernel[i];
        }

        u_output[pixel] = blurredMoments;
    }
}
