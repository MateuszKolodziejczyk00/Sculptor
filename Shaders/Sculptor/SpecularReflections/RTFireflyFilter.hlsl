#include "SculptorShader.hlsli"

[[descriptor_set(RTFireflyFilterDS, 0)]]

#include "SpecularReflections/SRReservoir.hlsli"


#define GROUP_SIZE_X 16
#define GROUP_SIZE_Y 16


struct CS_INPUT
{
	uint3 groupID  : SV_GroupID;
	uint3 localID  : SV_GroupThreadID;
	uint3 globalID : SV_DispatchThreadID;
};


#define GROUP_SIZE (GROUP_SIZE_X * GROUP_SIZE_Y)
#define WAVE_SIZE 32
#define WAVES_NUM_PER_GROUP (GROUP_SIZE / WAVE_SIZE)


groupshared float gs_reservoirWeightSumPerWave[WAVES_NUM_PER_GROUP];
groupshared uint  gs_validReservoirsNumPerWave[WAVES_NUM_PER_GROUP];

groupshared float gs_reservoirsAverageWeight;
groupshared uint  gs_validReservoirsNum;


[numthreads(GROUP_SIZE_X, GROUP_SIZE_Y, 1)]
void RTFireflyFilterCS(CS_INPUT input)
{
	const uint2 coords = input.globalID.xy;
	const bool isHelperLane = any(coords >= u_resamplingConstants.resolution);

	const uint reservoirIdx = GetScreenReservoirIdx(coords, u_resamplingConstants.reservoirsResolution);

	const SRPackedReservoir packedReservoir = u_inOutReservoirsBuffer[reservoirIdx];
	SRReservoir reservoir = UnpackReservoir(packedReservoir);

	const bool isSpecularTrace = reservoir.HasFlag(SR_RESERVOIR_FLAGS_SPECULAR_TRACE);

	const bool isValidReservoir = reservoir.IsValid() && !isHelperLane && !isSpecularTrace;

	const float reservoirWeight = isValidReservoir ? Luminance(reservoir.luminance) * reservoir.weightSum : 0.f;

	const float reservoirWeightSum = WaveActiveSum(reservoirWeight);
	const uint validReservoirsNum  = WaveActiveCountBits(isValidReservoir);

	const uint threadIdx = input.localID.x + GROUP_SIZE_X * input.localID.y;
	const uint waveIdx = threadIdx / WAVE_SIZE;

	if(WaveIsFirstLane())
	{
		gs_reservoirWeightSumPerWave[waveIdx] = reservoirWeightSum;
		gs_validReservoirsNumPerWave[waveIdx] = validReservoirsNum;
	}

	GroupMemoryBarrierWithGroupSync();

	if(waveIdx == 0u)
	{
		const float waveReservoirWeightSum = threadIdx < WAVES_NUM_PER_GROUP ? gs_reservoirWeightSumPerWave[threadIdx] : 0.f;
		const uint waveValidReservoirsNum  = threadIdx < WAVES_NUM_PER_GROUP ? gs_validReservoirsNumPerWave[threadIdx] : 0u;

		const uint validReservoirsNum =	WaveActiveSum(waveValidReservoirsNum);
		const float averageWeight     = WaveActiveSum(waveReservoirWeightSum) / max(validReservoirsNum, 1u);

		if(WaveIsFirstLane())
		{
			gs_reservoirsAverageWeight = averageWeight;
			gs_validReservoirsNum      = validReservoirsNum;
		}
	}

	GroupMemoryBarrierWithGroupSync();

	if(isValidReservoir)
	{
		const uint requiredValidReservoirsNum = GROUP_SIZE_X * GROUP_SIZE_Y / 3u;
		if(gs_validReservoirsNum > requiredValidReservoirsNum)
		{
			const float avgWeight         = gs_reservoirsAverageWeight;
			const float maxWeight         = avgWeight * 40.f;
			const float minVolatileWeight = avgWeight * 20.f;

			if(reservoirWeight > maxWeight)
			{
				reservoir.weightSum = 0.f;

				u_inOutReservoirsBuffer[reservoirIdx] = PackReservoir(reservoir);
			}
			else if(reservoirWeight > minVolatileWeight)
			{
				u_inOutReservoirsBuffer[reservoirIdx].MAndProps = AddPackedFlag(packedReservoir.MAndProps, SR_RESERVOIR_FLAGS_VOLATILE);
			}
		}
	}
}
