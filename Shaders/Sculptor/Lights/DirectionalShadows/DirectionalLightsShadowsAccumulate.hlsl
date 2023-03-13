#include "SculptorShader.hlsli"

[[descriptor_set(DirShadowsAccumulationViewDS, 0)]]
[[descriptor_set(DirShadowsAccumulationMasksDS, 1)]]


struct CS_INPUT
{
    uint3 globalID : SV_DispatchThreadID;
};


[numthreads(8, 8, 1)]
void AccumulateShadowsCS(CS_INPUT input)
{
    const uint2 pixel = input.globalID.xy;

    uint2 outputRes;
    u_shadowMask.GetDimensions(outputRes.x, outputRes.y);

    if(pixel.x < outputRes.x && pixel.y < outputRes.y)
    {
        const float2 uv = (float2(pixel) + 0.5f) / float2(outputRes);

        const float2 velocity = u_velocity.SampleLevel(u_velocitySampler, uv, 0);

        const float2 prevUV = uv - velocity;

        if (all(prevUV >= 0.f) && all(prevUV <= 1.f))
        {
            const float prevShadow = u_prevShadowMask.SampleLevel(u_shadowMaskSampler, prevUV, 0);

            const float currentShadow = u_shadowMask[pixel];

            const float exponentialAverageMultiplier = 0.12f;
            const float avgShadow = (currentShadow * exponentialAverageMultiplier) + (prevShadow * (1.f - exponentialAverageMultiplier));
            u_shadowMask[pixel] = avgShadow;
        }
    }
}
