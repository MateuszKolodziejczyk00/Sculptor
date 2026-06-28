#pragma once

#include "SculptorCoreTypes.h"


namespace spt::rsc
{

struct PlacementEntry
{
	math::Vector3f location;
	Real32         scale;
	Uint32         seed;
	Uint32         prefabIdx;
	Uint32         entryIdx;
};


struct PlacementProcessData
{
	lib::Span<const PlacementEntry> placements;
	Uint32 placementDefIdx = 0u;
};


using PlacementProcessor = lib::RawCallable<void(void* customData, const PlacementProcessData& data)>;


BEGIN_SHADER_STRUCT(PlacedPrefabDef)
	SHADER_STRUCT_FIELD(Real32, spawnProbability)
	SHADER_STRUCT_FIELD(Uint32, prefabIdx)
END_SHADER_STRUCT();


BEGIN_SHADER_STRUCT(PlacementPrefabsCollection)
	SHADER_STRUCT_FIELD(gfx::TypedBuffer<PlacedPrefabDef>, prefabs)
	SHADER_STRUCT_FIELD(Uint32,                            prefabsNum)
END_SHADER_STRUCT();


struct PlacementDefinition
{
	PlacementPrefabsCollection prefabsCollection;
	Real32                     placementSpacing = 1.f;
	Uint32                     resolution = 64u;
	// TODO: Rules shader
};


struct BiomeDefinition
{
	lib::DynamicArray<PlacementDefinition> placementDefinitions;
};


struct PlacementCommand
{
	BiomeDefinition*     biome = nullptr;
	Uint32               placementDefIdx = 0u;

	math::Vector2f                center = math::Vector2f(0.f, 0.f);
	std::optional<math::Vector2f> lastCenter;
};

} // spt::rsc
