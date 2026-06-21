#include "SculptorShader.hlsli"

[[descriptor_set(RenderSceneDS)]]
[[shader_params(PlacementConstants, u_constants)]]

#include "Terrain/SceneTerrain.hlsli"
#include "Utils/Random.hlsli"


struct CS_INPUT
{
	uint3 globalID : SV_DispatchThreadID;
	uint3 groupID  : SV_GroupID;
	uint3 localID  : SV_GroupThreadID;
};


bool SelectPrefab(in PlacementPrefabsCollection collection, inout RngState rngState, out PlacedPrefabDef prefabDef)
{
	const uint prefabsNum = collection.prefabsNum;

	float randomValue = rngState.Next();

	uint idx = 0u;
	while (randomValue > 0.f && idx < prefabsNum)
	{
		const PlacedPrefabDef prefab = collection.prefabs.Load(idx++);

		randomValue -= prefab.spawnProbability;

		if (randomValue <= 0.f)
		{
			prefabDef = prefab;
			return true;
		}
	}

	return false;
}


[numthreads(8, 8, 1)]
void ComputePlacementsCS(CS_INPUT input)
{
	const int2 globalCoords = u_constants.beginCoords + int2(input.globalID.xy);

	if (any(globalCoords >= u_constants.endCoords))
	{
		return;
	}

	RngState rngState = RngState::Create(uint2(globalCoords), 0u);
	const float2 location2d = (float2(globalCoords) + float2(rngState.Next(), rngState.Next())) * u_constants.placementSpacing;

	const TerrainInterface terrain = SceneTerrain();

	const float height = terrain.GetHeight(location2d);

	PlacedPrefabDef prefabDef;
	if (SelectPrefab(u_constants.prefabsCollection, rngState, prefabDef))
	{
		GPUPlacementEntry entry;
		entry.location  = float3(location2d, height);
		entry.scale     = 3.f + rngState.Next() * 4.f;
		entry.seed      = 0u;
		entry.prefabIdx = prefabDef.prefabIdx;

		const uint entryIdx = u_constants.rwEntriesNum.AtomicAdd(0u, 1u);
		u_constants.rwEntries.Store(entryIdx, entry);
	}
}
