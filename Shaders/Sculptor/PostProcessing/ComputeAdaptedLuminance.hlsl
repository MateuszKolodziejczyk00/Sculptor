#include "SculptorShader.hlsli"

[[descriptor_set(ComputeAdaptedLuminanceDS, 0)]]

#include "Utils/Exposure.hlsli"


// Based on https://bruop.github.io/exposure/


#define GROUP_SIZE 256

struct CS_INPUT
{
	uint3 globalID  : SV_DispatchThreadID;
	uint3 localID   : SV_GroupThreadID;
};


groupshared uint groupHistogramBins[GROUP_SIZE];

groupshared uint currentBin32Idx;

groupshared uint groupHistogramBinsPrefix[GROUP_SIZE];
groupshared uint groupHistogramBinsPrefixGroup[GROUP_SIZE / 32];


[numthreads(GROUP_SIZE, 1, 1)]
void AdaptedLuminanceCS(CS_INPUT input)
{
	const uint histogramLocalBinIdx = input.localID.x;
	uint countForLocalBin = u_luminanceHistogram[histogramLocalBinIdx];
	groupHistogramBins[histogramLocalBinIdx] = countForLocalBin;

	if (input.localID.x == 0)
	{
		currentBin32Idx = 0;
	}

	GroupMemoryBarrierWithGroupSync();

	{
		uint currentBin32;
		if (WaveIsFirstLane())
		{
			InterlockedAdd(currentBin32Idx, 1, OUT currentBin32);
		}

		currentBin32 = WaveReadLaneFirst(currentBin32);

		const uint currentVal = groupHistogramBins[currentBin32 * 32 + WaveGetLaneIndex()];
	
		const uint localPrefix = WavePrefixSum(currentVal);

		if (WaveGetLaneIndex() == 31)
		{
			groupHistogramBinsPrefixGroup[currentBin32] = localPrefix + currentVal;
		}

		GroupMemoryBarrierWithGroupSync();

		if(currentBin32 == 0)
		{
			if (WaveGetLaneIndex() < GROUP_SIZE / 32)
			{
				const uint groupPrefix = groupHistogramBinsPrefixGroup[WaveGetLaneIndex()];
				groupHistogramBinsPrefixGroup[WaveGetLaneIndex()] = WavePrefixSum(groupPrefix) + groupPrefix;
			}
		}

		GroupMemoryBarrierWithGroupSync();
	
		const uint groupPrefix = currentBin32 > 0 ? groupHistogramBinsPrefixGroup[currentBin32 - 1] : 0u;
		groupHistogramBinsPrefix[currentBin32 * 32 + WaveGetLaneIndex()] = groupPrefix + localPrefix;
	}

	GroupMemoryBarrierWithGroupSync();

	const float pixelsNum = float(u_exposureSettings.textureSize.x * u_exposureSettings.textureSize.y);

	const float rejectedDarkPixelsNum   = pixelsNum * u_exposureSettings.rejectedDarkPixelsPercentage;
	const float rejectedBrightPixelsNum = pixelsNum * u_exposureSettings.rejectedBrightPixelsPercentage;
	const float importantPixelsNum      = pixelsNum - rejectedDarkPixelsNum - rejectedBrightPixelsNum;

	const uint prefix = groupHistogramBinsPrefix[histogramLocalBinIdx];
	const uint suffix = pixelsNum - prefix - countForLocalBin;

	countForLocalBin = clamp(prefix + countForLocalBin - rejectedDarkPixelsNum, 0.f, countForLocalBin);
	countForLocalBin = clamp(suffix + countForLocalBin - rejectedBrightPixelsNum, 0.f, countForLocalBin);

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
		const float weightedSum = groupHistogramBins[0];
		const float pixelsWithNonZeroLuminance = max(importantPixelsNum - float(countForLocalBin), 1.0);

		const float weightedLogAverage = (weightedSum / pixelsWithNonZeroLuminance) - 1.0;

		// Map from our histogram space to actual luminance
		const float weightedAvgLuminance = exp2(((weightedLogAverage / 254.0) * u_exposureSettings.logLuminanceRange) + u_exposureSettings.minLogLuminance);

		const float luminanceLastFrame = u_adaptedLuminance[0];

		const float adaptationThisFrame = min(u_exposureSettings.deltaTime * u_exposureSettings.adaptationSpeed, 1.f);
		const float adaptedLuminance = luminanceLastFrame + (weightedAvgLuminance - luminanceLastFrame) * adaptationThisFrame;

		u_adaptedLuminance[0] = adaptedLuminance;

		const float EV100 = ComputeEV100FromAvgLuminance(adaptedLuminance + 0.001f);
		const float exposure = ConvertEV100ToExposure(EV100);

		ViewExposureData viewExposureData = u_viewExposureData[0];
		viewExposureData.exposureLastFrame = viewExposureData.exposure;
		viewExposureData.rcpExposureLastFrame = viewExposureData.rcpExposure;
		viewExposureData.exposure          = exposure;
		viewExposureData.rcpExposure       = rcp(viewExposureData.exposure);
		viewExposureData.EV100             = EV100;

		u_viewExposureData[0] = viewExposureData;
	}
}