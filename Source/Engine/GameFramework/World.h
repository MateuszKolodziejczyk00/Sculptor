#pragma once

#include "GameFrameworkMacros.h"
#include "PrefabInstance.h"
#include "SculptorCoreTypes.h"
#include "EngineFrame.h"


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

	struct WorldPrefabs&       prefabs;
	struct WorldMeshes&        meshes;
	struct WorldMaterialSlots& materials;

	PrefabInstanceHandle SpawnPrefab(const as::PrefabAssetHandle& prefab, const PrefabSpawnParams& params);

	void BeginFrame(engn::FrameContext& frame);

private:

	void UpdateRenderScene(engn::FrameContext& frame);

	lib::SharedPtr<rsc::RenderScene> m_renderScene;
};

} // spt::gf
