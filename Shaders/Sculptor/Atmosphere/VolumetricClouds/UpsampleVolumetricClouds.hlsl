#include "SculptorShader.hlsli"

[[shader_params(VolumetricCloudsUpsampleConstants, PARAMS_UPSAMPLE_VOLUMETRIC_CLOUDS)]]


struct CS_INPUT
{
    uint3 globalID : SV_DispatchThreadID;
};


[numthreads(8, 8, 1)]
void UpsampleVolumetricCloudsCS(CS_INPUT input)
{
    const uint2 coords = input.globalID.xy;

    const float2 inputUV = (coords + 0.5f) * PARAMS_UPSAMPLE_VOLUMETRIC_CLOUDS->rcpResolution;

    const float2 inputPixelSize = 2.f * PARAMS_UPSAMPLE_VOLUMETRIC_CLOUDS->rcpResolution;

    const uint2 noiseCoords = coords & 255u;
    const float noise = frac(PARAMS_UPSAMPLE_VOLUMETRIC_CLOUDS->blueNoise256.Load(uint3(noiseCoords, 0u)) + PARAMS_UPSAMPLE_VOLUMETRIC_CLOUDS->frameIdx * SPT_GOLDEN_RATIO);
    const float2 noiseOffset = float2(frac(noise * 2.f), noise) * 2.f - 1.f;

    const float2 noiseOffsetUV = noiseOffset * inputPixelSize * 0.4f;

    const float2 sourceUV = inputUV + noiseOffsetUV;

    const float4 upsampledCloud = PARAMS_UPSAMPLE_VOLUMETRIC_CLOUDS->cloudsHalfRes.SampleLevel(BindlessSamplers::LinearClampEdge(), sourceUV, 0.f);

    const float4 cloudDepth4 = PARAMS_UPSAMPLE_VOLUMETRIC_CLOUDS->cloudsDepthHalfRes.GatherRed(BindlessSamplers::LinearClampEdge(), sourceUV);

    // Add a bit of noise to help temporal upsamples (as those upsamples don't like 2x2 features). This is kind of similar to stochastic upsampling from Stachowiak 2015 presentation about SSR
    const float2 sourcePixel = sourceUV * (PARAMS_UPSAMPLE_VOLUMETRIC_CLOUDS->resolution * 0.5f);
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

    PARAMS_UPSAMPLE_VOLUMETRIC_CLOUDS->rwClouds[coords]      = upsampledCloud;
    PARAMS_UPSAMPLE_VOLUMETRIC_CLOUDS->rwCloudsDepth[coords] = upsampledCloudDepth;
}
