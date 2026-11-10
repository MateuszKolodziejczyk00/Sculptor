#include "SculptorShader.hlsli"

[[shader_params(DDGIUpdateProbesAverageLuminanceConsts, CONSTS)]]

#include "DDGI/DDGITypes.hlsli"


struct CS_INPUT
{
	uint3 groupID : SV_GroupID;
};


[numthreads(32, 1, 1)]
void DDGIUpdateProbesAverageLuminanceCS(CS_INPUT input)
{
	const uint updatedProbeIdx = input.groupID.x;

	const uint3 updatedProbeCoords = ComputeUpdatedProbeCoords(updatedProbeIdx, CONSTS->relitParams->probesToUpdateCoords, CONSTS->relitParams->probesToUpdateCount);
	const uint3 probeWrappedCoords = ComputeProbeWrappedCoords(*CONSTS->volumeParams, updatedProbeCoords);

	// add 1 because of the border
	const DDGIProbeDataCoords probeDataCoords = ComputeProbeIlluminanceDataOffset(*CONSTS->volumeParams, probeWrappedCoords);

	SRVTexture2D<float3> probeIlluminanceTexture = CONSTS->volumeIlluminanceTextures[probeDataCoords.textureIdx];
	const uint2 probeDataOffset = probeDataCoords.textureLocalCoords + 1;

	const uint samplesNum = CONSTS->volumeParams->probeIlluminanceDataRes.x * CONSTS->volumeParams->probeIlluminanceDataRes.y;

	float3 averageLuminance = 0.f;
	for (uint waveSampleIdx = 0; waveSampleIdx < samplesNum; waveSampleIdx += WaveGetLaneCount())
	{
		float3 sampleLuminance = 0.f;

		const uint sampleIdx = waveSampleIdx + WaveGetLaneIndex();
		if(sampleIdx < samplesNum)
		{
			uint2 sampleOffset;
			sampleOffset.y = sampleIdx / CONSTS->volumeParams->probeIlluminanceDataRes.x;
			sampleOffset.x = sampleIdx - sampleOffset.y * CONSTS->volumeParams->probeIlluminanceDataRes.x;
			sampleLuminance = probeIlluminanceTexture.Load(int3(probeDataOffset + sampleOffset, 0)).xyz;
		}

		const float3 samplesLumianceSum = WaveActiveSum(sampleLuminance);
		averageLuminance += samplesLumianceSum;
	}

	averageLuminance /= samplesNum;

	if(WaveIsFirstLane())
	{
		// We need to multiply by 2 because probes store only cos weighted luminance. Multiplication gives average luminance for the whole hemisphere, without weighting.
		CONSTS->probesAverageLuminanceTexture[probeWrappedCoords] = averageLuminance * 2.f;
	}
}
