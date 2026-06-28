#pragma once

#include "GameFrameworkMacros.h"
#include "PrefabInstance.h"
#include "SculptorCoreTypes.h"
#include "EngineFrame.h"
#include "PlacementSystem/WorldPlacementSystem.h"
#include "TerrainAsset.h"


namespace spt::rsc
{
class RenderScene;
} // spt::rsc


namespace spt::gf
{

struct PrefabSpawnParams
{
	math::Vector3f location = math::Vector3f::Zero();
	math::Vector3f rotation = math::Vector3f::Zero();
	math::Vector3f scale    = math::Vector3f::Ones();
};


class World
{
public:

	World();
	~World();

	const lib::SharedPtr<rsc::RenderScene>& GetRenderScene() const { return m_renderScene; }

	rsc::RenderScene&       GetRenderSceneRef()       { return *m_renderScene; }
	const rsc::RenderScene& GetRenderSceneRef() const { return *m_renderScene; }

	void BeginFrame(engn::FrameContext& frame);

	PrefabInstanceHandle SpawnPrefab(const as::PrefabAssetHandle& prefab, const PrefabSpawnParams& params);
	void                 DestroyPrefabInstance(PrefabInstanceHandle instanceHandle);

	void                  SetBiome(const rsc::BiomeDefinition& biome, lib::DynamicArray<as::PrefabAssetHandle> placementAssets);
	void                  ProcessPlacements(rsc::SceneRendererHandle sceneRenderer);
	rsc::PlacementCommand CreatePlacementCommand(engn::FrameContext& frame, math::Vector3f location);

	void                   SetTerrain(as::TerrainAssetHandle terrainAsset);
	as::TerrainAssetHandle GetTerrainAsset() const { return m_terrainAsset; }

	struct WorldPrefabs&       prefabs;
	struct WorldMeshes&        meshes;
	struct WorldMaterialSlots& materials;

private:

	void UpdateRenderScene(engn::FrameContext& frame);

	lib::SharedPtr<rsc::RenderScene> m_renderScene;

	WorldPlacementSystem m_placementSystem;

	as::TerrainAssetHandle m_terrainAsset;
};

} // spt::gf
