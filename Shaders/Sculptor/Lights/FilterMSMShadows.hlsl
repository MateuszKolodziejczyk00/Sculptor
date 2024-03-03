#include "SculptorShader.hlsli"

[[descriptor_set(FilterMSMDS, 0)]]

#include "Utils/SceneViewUtils.hlsli"


#ifndef IS_HORIZONTAL
#error "IS_HORIZONTAL is not defined"
#endif // IS_HORIZONTAL


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

groupshared float4 g_grupInput[SHARED_DATA_ELEMENTS_NUM];


#if IS_HORIZONTAL

#define GROUP_SIZE_X GROUP_SIZE
#define GROUP_SIZE_Y 1

#else // IS_HORIZONTAL

#define GROUP_SIZE_X 1
#define GROUP_SIZE_Y GROUP_SIZE

#endif // IS_HORIZONTAL


// Based on MJP implementation: https://github.com/TheRealMJP/Shadows/blob/master/Shadows/MSM.hlsl
float4 GetOptimizedMoments(in float depth)
{
    float square = depth * depth;
    float4 moments = float4(depth, square, square * depth, square * square);
    float4 optimized = mul(moments, float4x4(-2.07224649f,    13.7948857237f,  0.105877704f,   9.7924062118f,
                                              32.23703778f,  -59.4683975703f, -1.9077466311f, -33.7652110555f,
                                             -68.571074599f,  82.0359750338f,  9.3496555107f,  47.9456096605f,
                                              39.3703274134f,-35.364903257f,  -6.6543490743f, -23.9728048165f));
    optimized[0] += 0.035955884801f;
    return optimized;
}


[numthreads(GROUP_SIZE_X, GROUP_SIZE_Y, 1)]
void FilterMSMShadowMapCS(CS_INPUT input)
{
    int2 outputRes;
    u_output.GetDimensions(outputRes.x, outputRes.y);

    const float2 pixelSize = rcp(float2(outputRes));

    const int2 pixelsDirection = IS_HORIZONTAL ? int2(1, 0) : int2(0, 1);

    const int2 groupBeginPixel = input.groupID.xy * int2(GROUP_SIZE_X, GROUP_SIZE_Y);

    const int localIdx = IS_HORIZONTAL ? input.localID.x : input.localID.y;
    
    for (int i = localIdx; i < SHARED_DATA_ELEMENTS_NUM; i += GROUP_SIZE)
    {
        const int2 offset = (i - HALF_KERNEL) * pixelsDirection;
        const int2 pixel = clamp(groupBeginPixel + offset, 0, outputRes);

        const float2 uv = pixel * pixelSize;

 #if IS_HORIZONTAL

        float depth = u_input.SampleLevel(u_inputSampler, uv, 0).x;

        if(u_params.linearizeDepth)
        {
            float p20, p23;
            ComputeShadowProjectionParams(u_params.nearPlane, u_params.farPlane, p20, p23);

            depth = ComputeShadowLinearDepth(depth, p20, p23) / u_params.farPlane;
        }

        g_grupInput[i] = GetOptimizedMoments(depth);

#else // IS_HORIZONTAL

        g_grupInput[i] = u_input.SampleLevel(u_inputSampler, uv, 0);

#endif // IS_HORIZONTAL
    }

    GroupMemoryBarrierWithGroupSync();
   
    const int2 pixel = input.globalID.xy;

    if(pixel.x < outputRes.x && pixel.y < outputRes.y)
    {
        float4 blurredMoments = 0.f;
        
        for (int i = 0; i <= HALF_KERNEL * 2; ++i)
        {
            blurredMoments += g_grupInput[localIdx + i] * kernel[i];
        }

        u_output[pixel] = blurredMoments;
    }
}
