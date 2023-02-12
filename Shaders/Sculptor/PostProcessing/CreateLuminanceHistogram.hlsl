#include "SculptorShader.hlsli"

[[descriptor_set(LuminanceHistogramDS, 0)]]

// Based on https://bruop.github.io/exposure/


#define EPSILON 0.005


struct CS_INPUT
{
    uint3 globalID  : SV_DispatchThreadID;
    uint3 localID   : SV_GroupThreadID;
};


groupshared uint groupHistogramBins[256];


uint LuminanceToBinIdx(float luminance, float minLogLuminance, float inverseLogLuminanceRange)
{
    // Avoid taking the log of zero
    if (luminance < EPSILON)
    {
        return 0;
    }
    
    const float logLuminance = saturate((log2(luminance) - minLogLuminance) * inverseLogLuminanceRange);
    
    // Map [0, 1] to [1, 255]. The zeroth bin is handled by the epsilon check above.
    return uint(logLuminance * 254.0 + 1.0);
}


[numthreads(16, 16, 1)]
void LuminanceHistogramCS(CS_INPUT input)
{
    const uint histogramLocalBinIdx = input.localID.x + input.localID.y * 16;
    groupHistogramBins[histogramLocalBinIdx] = 0;

    GroupMemoryBarrierWithGroupSync();
    
    const uint2 pixel = input.globalID.xy;

    if(pixel.x < u_exposureSettings.textureSize.x && pixel.y < u_exposureSettings.textureSize.y)
    {
        const float2 uv = pixel * u_exposureSettings.inputPixelSize + u_exposureSettings.inputPixelSize * 0.5f;
        const float3 radiance = u_radianceTexture.SampleLevel(u_sampler, uv, 0).xyz;

        const float3 luminance = Luminance(radiance);

        const uint binIdx = LuminanceToBinIdx(luminance, u_exposureSettings.minLogLuminance, u_exposureSettings.inverseLogLuminanceRange);

        InterlockedAdd(groupHistogramBins[binIdx], 1);
    }

    GroupMemoryBarrierWithGroupSync();

    InterlockedAdd(u_luminanceHistogram[histogramLocalBinIdx], groupHistogramBins[histogramLocalBinIdx]);
}