#include "SculptorShader.hlsli"

[[shader_params(BuildLightsGridConstants, CONSTS)]]

#include "Lights/LightingUtils.hlsli"
#include "Lights/LightsGrid.hlsli"


struct CS_INPUT
{
	uint3 groupID : SV_GroupID;
	uint3 localID : SV_GroupThreadID;
};


bool DoesLightAffectCell(in uint lightIdx, in float3 cellMin, in float3 cellMax)
{
	if (lightIdx >= CONSTS->localLightsNum)
	{
		return false;
	}

	const LocalLightInterface light = CONSTS->localLights[lightIdx];

	float3 center;
	float radius;
	CreateLightBoundingSphere(light, OUT center, OUT radius);

	return light.ContributesToDI() && Dist2(clamp(center, cellMin, cellMax), center) < Pow2(radius);
}


[numthreads(32, 1, 1)]
void BuildLightsGridCS(CS_INPUT input)
{
	const uint threadIdx  = input.localID.x;
	const uint cascadeIdx = input.groupID.z / LightsGridCascadeRes.z;
	const uint3 coords    = uint3(input.groupID.xy, input.groupID.z % LightsGridCascadeRes.z);

	float3 cellMin;
	float3 cellMax;
	CONSTS->cascades[cascadeIdx].GetCellBounds(coords, OUT cellMin, OUT cellMax);

	uint lightsNum = 0u;
	for (uint chunkBegin = 0u; chunkBegin < CONSTS->localLightsNum; chunkBegin += 32u)
	{
		lightsNum += WaveActiveCountBits(DoesLightAffectCell(chunkBegin + threadIdx, cellMin, cellMax));
	}

	// Node 0 is a shared empty list
	uint firstNodeIdx = 0u;
	if (lightsNum > 0u && WaveIsFirstLane())
	{
		const uint allocatedNodeIdx = CONSTS->rwNodesCounter.AtomicAdd(0u, lightsNum + 1u);
		if (allocatedNodeIdx + lightsNum + 1u <= LIGHTS_GRID_NODES_CAPACITY)
		{
			firstNodeIdx = allocatedNodeIdx;
		}
	}
	firstNodeIdx = WaveReadLaneFirst(firstNodeIdx);

	if (WaveIsFirstLane())
	{
		CONSTS->rwCells.Store(GetLightsGridCellIdx(cascadeIdx, coords), uint16_t(firstNodeIdx));
	}

	if (firstNodeIdx == 0u)
	{
		return;
	}

	// Lights are written in ascending index order, so shading sums contributions in the same order as a loop over all lights
	uint nodeIdx = firstNodeIdx;
	for (uint writeChunkBegin = 0u; writeChunkBegin < CONSTS->localLightsNum; writeChunkBegin += 32u)
	{
		const bool affectsCell = DoesLightAffectCell(writeChunkBegin + threadIdx, cellMin, cellMax);
		if (affectsCell)
		{
			CONSTS->rwNodes.Store(nodeIdx + WavePrefixCountBits(affectsCell), uint16_t(writeChunkBegin + threadIdx));
		}
		nodeIdx += WaveActiveCountBits(affectsCell);
	}

	if (WaveIsFirstLane())
	{
		CONSTS->rwNodes.Store(nodeIdx, uint16_t(LIGHTS_GRID_LIST_END));
	}
}
