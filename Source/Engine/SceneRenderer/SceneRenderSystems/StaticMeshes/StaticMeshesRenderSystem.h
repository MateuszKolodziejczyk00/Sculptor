#pragma once

#include "SceneRenderSystems/SceneRenderSystem.h"
#include "Utils/Geometry/GeometryTypes.h"
#include "Utils/Geometry/StaticMeshRenderingCommon.h"
#include "Utils/Geometry/StaticMeshShadowMapRenderer.h"


namespace spt::rsc
{

class RenderView;
struct PointLightData;


class RENDER_SCENE_API StaticMeshesRenderSystem : public SceneRenderSystem
{
protected:

	using Super = SceneRenderSystem;

public:

	static constexpr ESceneRenderSystem systemType = ESceneRenderSystem::StaticMeshesSystem;
	
	explicit StaticMeshesRenderSystem(RenderScene& owningScene);

	// Begin SceneRenderSystem overrides
	void Update(const SceneUpdateContext& context);
	void RenderPerFrame(rg::RenderGraphBuilder& graphBuilder, const SceneRendererInterface& rendererInterface, const RenderScene& renderScene, const lib::DynamicPushArray<ViewRenderingSpec*>& viewSpecs, const SceneRendererSettings& settings);
	void FinishRenderingFrame(rg::RenderGraphBuilder& graphBuilder, const SceneRendererInterface& rendererInterface, const RenderScene& renderScene);
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
