#include "SculptorShader.hlsli"

[[descriptor_set(DDGIUpdateProbesAverageLuminanceDS, 0)]]

#include "DDGI/DDGItypes.hlsli"


struct CS_INPUT
{
	uint3 groupID : SV_GroupID;
	uint3 localID : SV_GroupThreadID;
};


[numthreads(32, 1, 1)]
void DDGIUpdateProbesAverageLuminanceCS(CS_INPUT input)
{
	const uint updatedProbeIdx = input.groupID.x;

	const uint2 localID = input.localID.xy;

	const uint3 updatedProbeCoords = ComputeUpdatedProbeCoords(updatedProbeIdx, u_relitParams.probesToUpdateCoords, u_relitParams.probesToUpdateCount);
	const uint3 probeWrappedCoords = ComputeProbeWrappedCoords(u_volumeParams, updatedProbeCoords);

	// add 1 because of the border
	const uint2 probeDataOffset = ComputeProbeIlluminanceDataOffset(u_volumeParams, probeWrappedCoords) + 1;

	const uint samplesNum = u_volumeParams.probeIlluminanceDataRes.x * u_volumeParams.probeIlluminanceDataRes.y;

	float3 averageLuminance = 0.f;
	for (uint waveSampleIdx = 0; waveSampleIdx < samplesNum; waveSampleIdx += WaveGetLaneCount())
	{
		float3 sampleLuminance = 0.f;

		const uint sampleIdx = waveSampleIdx + WaveGetLaneIndex();
		if(sampleIdx < samplesNum)
		{
			uint2 sampleOffset;
			sampleOffset.y = sampleIdx / u_volumeParams.probeIlluminanceDataRes.x;
			sampleOffset.x = sampleIdx - sampleOffset.y * u_volumeParams.probeIlluminanceDataRes.x;
			sampleLuminance = u_probesIlluminanceTexture.Load(int3(probeDataOffset + sampleOffset, 0)).xyz;
		}

		const float3 samplesLumianceSum = WaveActiveSum(sampleLuminance);
		averageLuminance += samplesLumianceSum;
	}

	averageLuminance /= samplesNum;

	if(WaveIsFirstLane())
	{
		u_probesAverageLuminanceTexture[probeWrappedCoords] = averageLuminance;
	}
}
