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
};


using PlacementProcessor = lib::RawCallable<void(void* customData, lib::Span<const PlacementEntry>)>;


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


struct PlacementCommand
{
	PlacementDefinition* definition = nullptr;
	math::Vector2f       center = math::Vector2f(0.f, 0.f);
};


struct BiomeDefinition
{
	lib::DynamicArray<PlacementDefinition> placementDefinitions;
};

} // spt::rsc
