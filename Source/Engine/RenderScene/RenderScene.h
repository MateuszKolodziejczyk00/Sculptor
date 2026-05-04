#pragma once

#include "Lights/LightingScene.h"
#include "Materials/MaterialsRenderingCommon.h"
#include "RayTracing/RTScene.h"
#include "RenderSceneMacros.h"
#include "SculptorCoreTypes.h"
#include "RenderSceneRegistry.h"
#include "RenderSceneTypes.h"
#include "StaticMeshes/RetainedDraws.h"
#include "Terrain/TerrainDefinition.h"


namespace spt::engn
{
class FrameContext;
} // spt::engn


namespace spt::rsc
{

using RenderInstances = lib::PagedGenerationalPool<RenderInstance>;


struct RenderInstanceDef
{
	math::Affine3f transform;
};


class RENDER_SCENE_API RenderScene
{
public:

	RenderScene();

	void BeginFrame(const engn::FrameContext& frame);
	void EndFrame();

	const engn::FrameContext& GetCurrentFrameRef() const;

	// Instances ============================================================

	RenderInstanceHandle CreateInstance(const RenderInstanceDef& def);

	// Terrain ==============================================================

	void SetTerrainDefinition(const TerrainDefinition& definition);
	const TerrainDefinition& GetTerrainDefinition() const;

	// Rendering ============================================================

	const lib::SharedRef<rdr::Buffer>& GetRenderEntitiesBuffer() const;

	// Scene Data ===========================================================

	RTScene rt;

	SceneMaterials materials;

	RetainedDraws draws;

	LightingScene lighting;

	const RenderInstances& GetInstances() const { return m_instances; }

private:

	lib::SharedRef<rdr::Buffer> CreateInstancesBuffer() const;

	TerrainDefinition m_terrainDefinition;

	RenderInstances m_instances;

	lib::SharedRef<rdr::Buffer> m_renderEntitiesBuffer;

	lib::SharedPtr<const engn::FrameContext> m_currentFrame;
};

} // spt::rsc
