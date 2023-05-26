#include "SculptorShader.hlsli"

[[descriptor_set(LensFlaresBlurDS, 0)]]


struct CS_INPUT
{
    uint3 globalID : SV_DispatchThreadID;
    uint3 groupID : SV_GroupID;
    uint3 localID : SV_GroupThreadID;
};


#define BLUR_KERNEL 14

#define GROUP_SIZE 256

#define SHARED_DATA_SIZE (GROUP_SIZE + 2 * BLUR_KERNEL)

groupshared float3 sharedData[SHARED_DATA_SIZE];


float GaussianBlurWeight(int x, float sigma)
{
    const float sigmaSquared = Pow2(sigma);
    return exp(-0.5f * Pow2(x) / sigmaSquared) / (sqrt(2.f * PI * sigmaSquared));
}


[numthreads(GROUP_SIZE, 1, 1)]
void LensFlaresBlurCS(CS_INPUT input)
{
    const int2 pixel = u_blurParams.isHorizontal ? input.globalID.xy : input.globalID.yx;

    const int2 groupBeginPixel = u_blurParams.isHorizontal ? int2(input.groupID.x * GROUP_SIZE, input.groupID.y) : int2(input.groupID.y, input.groupID.x * GROUP_SIZE);

    uint2 outputRes;
    u_outputTexture.GetDimensions(outputRes.x, outputRes.y);

    const float2 outputResF = float2(outputRes);

    const int2 offset = u_blurParams.isHorizontal ? int2(1, 0) : int2(0, 1);
    for (int i = input.localID.x; i < SHARED_DATA_SIZE; i += GROUP_SIZE)
    {
        const int2 sample = groupBeginPixel + (i - BLUR_KERNEL) * offset;
        const float2 uv = saturate((sample + 0.5f) / outputResF);

        sharedData[i] = u_inputTexture.SampleLevel(u_inputSampler, uv, 0);
    }

    GroupMemoryBarrierWithGroupSync();

    if(pixel.x < outputRes.x && pixel.y < outputRes.y)
    {
        float3 value = 0.f;
        float weightSum = 0.f;
        
        for (int i = 0; i <= 2 * BLUR_KERNEL; ++i)
        {
            const float weight = GaussianBlurWeight(abs(i - BLUR_KERNEL), 8.f);
            value += sharedData[input.localID.x + i] * weight;
            weightSum += weight;
        }

        u_outputTexture[pixel] = value / weightSum;
    }
}
