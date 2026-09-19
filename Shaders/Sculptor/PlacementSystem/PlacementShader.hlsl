#include "SculptorShader.hlsli"

[[shader_params(RenderSceneConstants, SCENE)]]
[[shader_params(PlacementConstants, PARAMS_PLACEMENT_CONSTANTS)]]

#include "Terrain/SceneTerrain.hlsli"
#include "SceneRendering/GPUScene.hlsli"
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


uint GlobalCoordsToEntryIdx(in int2 globalCoords)
{
	const int2 modCoords = globalCoords % PARAMS_PLACEMENT_CONSTANTS->resolution;
	return modCoords.x + modCoords.y * PARAMS_PLACEMENT_CONSTANTS->resolution;
}


[numthreads(8, 8, 1)]
void ComputePlacementsCS(CS_INPUT input)
{
	const int2 globalCoords = PARAMS_PLACEMENT_CONSTANTS->beginCoords + int2(input.globalID.xy);

	if (any(globalCoords >= PARAMS_PLACEMENT_CONSTANTS->endCoords))
	{
		return;
	}

	if (PARAMS_PLACEMENT_CONSTANTS->lastCoordsValid)
	{
		if (all(globalCoords >= PARAMS_PLACEMENT_CONSTANTS->lastBeginCoords) && all(globalCoords < PARAMS_PLACEMENT_CONSTANTS->lastEndCoords))
		{
			return;
		}
	}

	RngState rngState = RngState::Create(uint2(globalCoords), 0u);
	const float2 location2d = (float2(globalCoords) + float2(rngState.Next(), rngState.Next())) * PARAMS_PLACEMENT_CONSTANTS->placementSpacing;

	const TerrainInterface terrain = SceneTerrain();

	const float height = terrain.GetHeight(location2d);

	PlacedPrefabDef prefabDef;
	bool isPrefabSelected = SelectPrefab(PARAMS_PLACEMENT_CONSTANTS->prefabsCollection, rngState, prefabDef);
	GPUPlacementEntry entry;
	entry.location  = float3(location2d, height);
	entry.scale     = 3.f + rngState.Next() * 4.f;
	entry.seed      = 0u;
	entry.prefabIdx = isPrefabSelected ? prefabDef.prefabIdx : IDX_NONE_32;
	entry.entryIdx  = GlobalCoordsToEntryIdx(globalCoords);

	const uint entryIdx = PARAMS_PLACEMENT_CONSTANTS->rwEntriesNum.AtomicAdd(0u, 1u);
	PARAMS_PLACEMENT_CONSTANTS->rwEntries.Store(entryIdx, entry);
}
