#pragma once

#include "SceneRenderSystems/SceneRenderSystem.h"
#include "TerrainTypes.h"
#include "RGResources/RGResourceHandles.h"
#include "MaterialShader.h"


namespace spt::rdr
{
class Buffer;
class TextureView;
} // spt::rdr


namespace spt::rsc
{

struct TerrainVisibilityRenderParams
{
	const ViewRenderingSpec& viewSpec;

	rg::RGTextureViewHandle depthTexture;
	rg::RGTextureViewHandle visibilityTexture;
};


struct TerrainShadowMapRenderParams
{
	const ViewRenderingSpec& viewSpec;

	rg::RGTextureViewHandle depthTexture;
};


class RENDER_SCENE_API TerrainRenderSystem : public SceneRenderSystem
{
protected:

	using Super = SceneRenderSystem;

public:

	static constexpr ESceneRenderSystem systemType = ESceneRenderSystem::TerrainSystem;

	explicit TerrainRenderSystem(RenderScene& owningScene);

	// Begin SceneRenderSystem overrides
	void Initialize(lib::MemoryArena& arena, RenderScene& renderScene);
	void Deinitialize(RenderScene& renderScene);
	void Update(const SceneUpdateContext& context);
	void UpdateGPUSceneData(RenderSceneConstants& sceneData);
	void RenderPerFrame(rg::RenderGraphBuilder& graphBuilder, const SceneRendererInterface& rendererInterface, const RenderScene& renderScene, const lib::DynamicPushArray<ViewRenderingSpec*>& viewSpecs, const SceneRendererSettings& settings);
	// End SceneRenderSystem overrides

	Bool IsEnabled() const;

	void RenderVisibilityBuffer(rg::RenderGraphBuilder& graphBuilder, const TerrainVisibilityRenderParams& params) const;
	void RenderShadowMap(rg::RenderGraphBuilder& graphBuilder, const TerrainShadowMapRenderParams& params) const;

	mat::MaterialShader GetTerrainMaterialShader() const { return m_terrainMaterialShader; }

private:

	lib::SharedPtr<rdr::Buffer> m_tilesBuffer;
	lib::SharedPtr<rdr::Buffer> m_meshletVerticesBuffer;
	lib::SharedPtr<rdr::Buffer> m_meshletIndicesBuffer;

	Bool m_initialized = false;

	mat::MaterialShader m_terrainMaterialShader;

	struct TerrainRenderInstance* m_renderInstance = nullptr;
};

} // spt::rsc
