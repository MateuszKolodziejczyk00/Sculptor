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
	void Update(const SceneUpdateContext& context);
	void UpdateGPUSceneData(RenderSceneConstants& sceneData);
	// End SceneRenderSystem overrides

	Bool IsEnabled() const;

	void RenderVisibilityBuffer(rg::RenderGraphBuilder& graphBuilder, const TerrainVisibilityRenderParams& params) const;
	void RenderShadowMap(rg::RenderGraphBuilder& graphBuilder, const TerrainShadowMapRenderParams& params) const;

	mat::MaterialShader GetTerrainMaterialShader() const { return m_terrainMaterialShader; }

private:

	lib::SharedPtr<rdr::Buffer> m_tilesBuffer;
	lib::SharedPtr<rdr::Buffer> m_meshletVerticesBuffer;
	lib::SharedPtr<rdr::Buffer> m_meshletIndicesBuffer;

	lib::SharedPtr<rdr::TextureView> m_heightMap;
	Bool m_initialized = false;

	mat::MaterialShader m_terrainMaterialShader;
};

} // spt::rsc
