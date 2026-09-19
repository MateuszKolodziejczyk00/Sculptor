#include "SculptorShader.hlsli"

[[shader_params(DDGIUpdateProbesAverageLuminanceConsts, PARAMS_D_D_G_I_UPDATE_PROBES_AVERAGE_LUMINANCE)]]

#include "DDGI/DDGITypes.hlsli"


struct CS_INPUT
{
	uint3 groupID : SV_GroupID;
};


[numthreads(32, 1, 1)]
void DDGIUpdateProbesAverageLuminanceCS(CS_INPUT input)
{
	const uint updatedProbeIdx = input.groupID.x;

	const uint3 updatedProbeCoords = ComputeUpdatedProbeCoords(updatedProbeIdx, PARAMS_D_D_G_I_UPDATE_PROBES_AVERAGE_LUMINANCE->relitParams->probesToUpdateCoords, PARAMS_D_D_G_I_UPDATE_PROBES_AVERAGE_LUMINANCE->relitParams->probesToUpdateCount);
	const uint3 probeWrappedCoords = ComputeProbeWrappedCoords(*PARAMS_D_D_G_I_UPDATE_PROBES_AVERAGE_LUMINANCE->volumeParams, updatedProbeCoords);

	// add 1 because of the border
	const DDGIProbeDataCoords probeDataCoords = ComputeProbeIlluminanceDataOffset(*PARAMS_D_D_G_I_UPDATE_PROBES_AVERAGE_LUMINANCE->volumeParams, probeWrappedCoords);

	SRVTexture2D<float3> probeIlluminanceTexture = PARAMS_D_D_G_I_UPDATE_PROBES_AVERAGE_LUMINANCE->volumeIlluminanceTextures[probeDataCoords.textureIdx];
	const uint2 probeDataOffset = probeDataCoords.textureLocalCoords + 1;

	const uint samplesNum = PARAMS_D_D_G_I_UPDATE_PROBES_AVERAGE_LUMINANCE->volumeParams->probeIlluminanceDataRes.x * PARAMS_D_D_G_I_UPDATE_PROBES_AVERAGE_LUMINANCE->volumeParams->probeIlluminanceDataRes.y;

	float3 averageLuminance = 0.f;
	for (uint waveSampleIdx = 0; waveSampleIdx < samplesNum; waveSampleIdx += WaveGetLaneCount())
	{
		float3 sampleLuminance = 0.f;

		const uint sampleIdx = waveSampleIdx + WaveGetLaneIndex();
		if(sampleIdx < samplesNum)
		{
			uint2 sampleOffset;
			sampleOffset.y = sampleIdx / PARAMS_D_D_G_I_UPDATE_PROBES_AVERAGE_LUMINANCE->volumeParams->probeIlluminanceDataRes.x;
			sampleOffset.x = sampleIdx - sampleOffset.y * PARAMS_D_D_G_I_UPDATE_PROBES_AVERAGE_LUMINANCE->volumeParams->probeIlluminanceDataRes.x;
			sampleLuminance = probeIlluminanceTexture.Load(int3(probeDataOffset + sampleOffset, 0)).xyz;
		}

		const float3 samplesLumianceSum = WaveActiveSum(sampleLuminance);
		averageLuminance += samplesLumianceSum;
	}

	averageLuminance /= samplesNum;

	if(WaveIsFirstLane())
	{
		PARAMS_D_D_G_I_UPDATE_PROBES_AVERAGE_LUMINANCE->probesAverageLuminanceTexture[probeWrappedCoords] = averageLuminance;
	}
}
