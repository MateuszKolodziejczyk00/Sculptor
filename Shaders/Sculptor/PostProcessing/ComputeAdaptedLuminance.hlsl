#include "SculptorShader.hlsli"

[[shader_params(ComputeAdaptedLuminanceConstants, CONSTS)]]

#include "Utils/Exposure.hlsli"


// Based on https://bruop.github.io/exposure/


struct CS_INPUT
{
	uint3 globalID  : SV_DispatchThreadID;
	uint3 localID   : SV_GroupThreadID;
};


#if MANUAL_EXPOSURE

[numthreads(1, 1, 1)]
void AdaptedLuminanceCS(CS_INPUT input)
{
	const float EV100 = CONSTS->manualEV100;
	const float exposure = ConvertEV100ToExposure(EV100);
	const float avgLuminance = EV100ToAvgLuminance(EV100);
	CONSTS->adaptedLuminance[0] = avgLuminance;

	const float averageLogLuminance = log2(avgLuminance);

	ViewExposureData viewExposureData = CONSTS->viewExposureData[0];

	viewExposureData.exposureLastFrame    = viewExposureData.exposure;
	viewExposureData.rcpExposureLastFrame = viewExposureData.rcpExposure;
	viewExposureData.exposure             = exposure;
	viewExposureData.rcpExposure          = rcp(viewExposureData.exposure);
	viewExposureData.EV100                = EV100;
	viewExposureData.averageLogLuminance  = averageLogLuminance;

	CONSTS->viewExposureData[0] = viewExposureData;
}

#else

#define GROUP_SIZE 256

groupshared uint groupHistogramBins[GROUP_SIZE];

groupshared uint currentBin32Idx;

groupshared uint groupHistogramBinsPrefix[GROUP_SIZE];
groupshared uint groupHistogramBinsPrefixGroup[GROUP_SIZE / 32];


[numthreads(GROUP_SIZE, 1, 1)]
void AdaptedLuminanceCS(CS_INPUT input)
{
	const uint histogramLocalBinIdx = input.localID.x;
	uint countForLocalBin = CONSTS->luminanceHistogram[histogramLocalBinIdx];
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

	const float pixelsNum = float(CONSTS->exposureSettings->textureSize.x * CONSTS->exposureSettings->textureSize.y);

	const float rejectedDarkPixelsNum   = pixelsNum * CONSTS->exposureSettings->rejectedDarkPixelsPercentage;
	const float rejectedBrightPixelsNum = pixelsNum * CONSTS->exposureSettings->rejectedBrightPixelsPercentage;
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
		const float averageLogLuminance = (weightedLogAverage / 254.0) * CONSTS->exposureSettings->logLuminanceRange + CONSTS->exposureSettings->minLogLuminance;

		const float weightedAvgLuminance = exp2(averageLogLuminance);

		const float luminanceLastFrame = CONSTS->adaptedLuminance[0];

		const float adaptationThisFrame = min(CONSTS->exposureSettings->deltaTime * CONSTS->exposureSettings->adaptationSpeed, 1.f);

		const float adaptedLuminance = CONSTS->isFirstFrame ? adaptationThisFrame : luminanceLastFrame + (weightedAvgLuminance - luminanceLastFrame) * adaptationThisFrame;

		CONSTS->adaptedLuminance[0] = adaptedLuminance;

		float EV100 = ComputeEV100FromAvgLuminance(adaptedLuminance + 0.001f);
		EV100 = EV100 + Remap(EV100, -2.f, 14.f, CONSTS->exposureSettings->ec0, CONSTS->exposureSettings->ec1);

		const float exposure = ConvertEV100ToExposure(EV100);

		ViewExposureData viewExposureData = CONSTS->viewExposureData[0];

		viewExposureData.exposureLastFrame    = viewExposureData.exposure;
		viewExposureData.rcpExposureLastFrame = viewExposureData.rcpExposure;
		viewExposureData.exposure             = exposure;
		viewExposureData.rcpExposure          = rcp(viewExposureData.exposure);
		viewExposureData.EV100                = EV100;
		viewExposureData.averageLogLuminance  = averageLogLuminance;

		CONSTS->viewExposureData[0] = viewExposureData;
	}
}

#endif 
