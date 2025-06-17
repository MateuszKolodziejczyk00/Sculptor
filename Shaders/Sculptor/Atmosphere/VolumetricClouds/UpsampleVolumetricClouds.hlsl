#include "SculptorShader.hlsli"

[[descriptor_set(UpsampleVolumetricCloudsDS, 0)]]


struct CS_INPUT
{
    uint3 globalID : SV_DispatchThreadID;
};


[numthreads(8, 8, 1)]
void UpsampleVolumetricCloudsCS(CS_INPUT input)
{
    const uint2 coords = input.globalID.xy;

    const float2 inputUV = (coords + 0.5f) * u_passConstants.rcpResolution;

    const float2 inputPixelSize = 2.f * u_passConstants.rcpResolution;

    const uint2 noiseCoords = coords & 255u;
    const float noise = frac(u_blueNoise256.Load(uint3(noiseCoords, 0u)) + u_passConstants.frameIdx * SPT_GOLDEN_RATIO);
    const float2 noiseOffset = float2(frac(noise * 2.f), noise) * 2.f - 1.f;

    const float2 noiseOffsetUV = noiseOffset * inputPixelSize * 0.4f;

    const float2 sourceUV = inputUV + noiseOffsetUV;

    const float4 upsampledCloud = u_cloudsHalfRes.SampleLevel(u_linearSampler, sourceUV, 0.f);

    const float4 cloudDepth4 = u_cloudsDepthHalfRes.GatherRed(u_linearSampler, sourceUV, 0);

    // Add a bit of noise to help temporal upsamples (as those upsamples don't like 2x2 features). This is kind of similar to stochastic upsampling from Stachowiak 2015 presentation about SSR
    const float2 sourcePixel = sourceUV * (u_passConstants.resolution * 0.5f);
    const float2 sourcePixelFrac = frac(sourcePixel);
    
    float weight[4];
    weight[0] = (1.f - sourcePixelFrac.x) * sourcePixelFrac.y;
    weight[1] = (sourcePixelFrac.x) * sourcePixelFrac.y;
    weight[2] = sourcePixelFrac.x * (1.f - sourcePixelFrac.y);
    weight[3] = (1.f - sourcePixelFrac.x) * (1.f - sourcePixelFrac.y);

    float upsampledCloudDepth = 0.f;
    float depthWeightSum = 0.f;
    for(uint i = 0u; i < 4u; ++i)
    {
        if(cloudDepth4[i] > 0.f)
        {
            upsampledCloudDepth += cloudDepth4[i] * weight[i];
            depthWeightSum += weight[i];
        }
    }

    upsampledCloudDepth = depthWeightSum > 0.f ? upsampledCloudDepth / depthWeightSum : -1.f;

    u_rwClouds[coords]      = upsampledCloud;
    u_rwCloudsDepth[coords] = upsampledCloudDepth;
}