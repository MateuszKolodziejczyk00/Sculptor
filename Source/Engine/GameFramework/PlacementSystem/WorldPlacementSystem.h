#pragma once

#include "GameFrameworkMacros.h"
#include "PrefabInstance.h"
#include "SceneRenderer/SceneRenderer.h"
#include "SculptorCoreTypes.h"
#include "PlacementSystem/PlacementSystemTypes.h"
#include "PrefabAsset.h"


namespace spt::gf
{

class World;


class WorldPlacementSystem
{
public:

	WorldPlacementSystem() = default;

	void                  SetBiome(const rsc::BiomeDefinition& biome, lib::DynamicArray<as::PrefabAssetHandle> placementAssets);
	void                  ProcessPlacements(gf::World& world, rsc::SceneRendererHandle sceneRenderer);
	rsc::PlacementCommand CreatePlacementCommand(engn::FrameContext& frame, math::Vector3f location);

private:

	rsc::BiomeDefinition                     m_biome;
	lib::DynamicArray<as::PrefabAssetHandle> m_placementAssets;

	struct PlacementEntryState
	{
		PrefabInstanceHandle prefabInstance; 
	};

	struct PlacementState
	{
		lib::DynamicArray<PlacementEntryState> entries;
		std::optional<math::Vector2f>          lastCommandLocation;
	};

	lib::DynamicArray<PlacementState> m_placementStates;
};

} // spt::gf
