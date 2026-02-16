#pragma once

#include "SceneRenderSystem.h"
#include "StaticMeshShadowMapRenderer.h"
#include "Geometry/GeometryTypes.h"


namespace spt::rsc
{

class RenderView;
struct PointLightData;


class RENDER_SCENE_API StaticMeshesRenderSystem : public SceneRenderSystem
{
protected:

	using Super = SceneRenderSystem;

public:
	
	explicit StaticMeshesRenderSystem(RenderScene& owningScene);

	// Begin SceneRenderSystem overrides
	virtual void Update(const SceneUpdateContext& context) override;
	virtual void RenderPerFrame(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, const lib::DynamicArray<ViewRenderingSpec*>& viewSpecs, const SceneRendererSettings& settings) override;
	virtual void FinishRenderingFrame(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene) override;
	// End SceneRenderSystem overrides

	const lib::DynamicArray<StaticMeshSMBatchDefinition>& BuildBatchesForSMView(const RenderView& view) const;
	lib::DynamicArray<StaticMeshSMBatchDefinition>        BuildBatchesForPointLightSM(const PointLightData& pointLight) const;

	const GeometryPassDataCollection& GetCachedOpaqueGeometryPassData() const;
	const GeometryPassDataCollection& GetCachedTransparentGeometryPassData() const;

private:

	struct CachedSMBatches
	{
		lib::DynamicArray<StaticMeshSMBatchDefinition> batches;

		GeometryPassDataCollection opaqueGeometryPassData;
		GeometryPassDataCollection transparentGeometryPassData;
	};

	void RenderPerView(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec);

	CachedSMBatches CacheStaticMeshBatches() const;

	StaticMeshShadowMapRenderer m_shadowMapRenderer;

	CachedSMBatches m_cachedSMBatches;
};

} // spt::rsc
