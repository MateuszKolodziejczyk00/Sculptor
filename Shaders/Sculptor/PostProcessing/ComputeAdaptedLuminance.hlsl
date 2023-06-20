#include "SculptorShader.hlsli"

[[descriptor_set(ComputeAdaptedLuminanceDS, 0)]]

// Based on https://bruop.github.io/exposure/


#define GROUP_SIZE 256

struct CS_INPUT
{
    uint3 globalID  : SV_DispatchThreadID;
    uint3 localID   : SV_GroupThreadID;
};


groupshared uint groupHistogramBins[GROUP_SIZE];


[numthreads(GROUP_SIZE, 1, 1)]
void AdaptedLuminanceCS(CS_INPUT input)
{
    const uint histogramLocalBinIdx = input.localID.x;
    const uint countForLocalBin = u_luminanceHistogram[histogramLocalBinIdx];
    groupHistogramBins[histogramLocalBinIdx] = countForLocalBin * histogramLocalBinIdx;

    GroupMemoryBarrierWithGroupSync();

    // Weighted count of the luminance range
    [unroll]
    for (uint cutoff = (GROUP_SIZE >> 1); cutoff > 0; cutoff >>= 1)
    {
        if (histogramLocalBinIdx < cutoff)
        {
            groupHistogramBins[histogramLocalBinIdx] += groupHistogramBins[histogramLocalBinIdx + cutoff];
        }

        GroupMemoryBarrierWithGroupSync();
    }

    if (input.localID.x == 0)
    {
        const float pixelsNum = float(u_exposureSettings.textureSize.x * u_exposureSettings.textureSize.y);
        
        const float weightedSum = groupHistogramBins[0];
        const float pixelsWithNonZeroLuminance = max(pixelsNum - float(countForLocalBin), 1.0);

        const float weightedLogAverage = (weightedSum / pixelsWithNonZeroLuminance) - 1.0;

        // Map from our histogram space to actual luminance
        const float weightedAvgLuminance = exp2(((weightedLogAverage / 254.0) * u_exposureSettings.logLuminanceRange) + u_exposureSettings.minLogLuminance);

        const float luminanceLastFrame = u_adaptedLuminance[0];

        const float adaptationThisFrame = min(u_exposureSettings.deltaTime * u_exposureSettings.adaptationSpeed, 1.f);
        const float adaptedLuminance = luminanceLastFrame + (weightedAvgLuminance - luminanceLastFrame) * adaptationThisFrame;

        u_adaptedLuminance[0] = adaptedLuminance;
    }
}