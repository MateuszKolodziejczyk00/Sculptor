#pragma once

#include "SceneRenderSystem.h"
#include "Pipelines/PipelineState.h"


namespace spt::rdr
{
class Buffer;
} // spt::rdr


namespace spt::rsc
{

class ShadowMapsDS;


struct DirectionalLightRuntimeData
{
	DirectionalLightRuntimeData()
		: directionalLightsNum(0)
	{ }

	void Reset()
	{
		directionalLightsNum = 0;
		directionalLightsBuffer.reset();
	}

	Uint32 directionalLightsNum;
	lib::SharedPtr<rdr::Buffer> directionalLightsBuffer;
};


class RENDER_SCENE_API LightsRenderSystem : public SceneRenderSystem
{
protected:

	using Super = SceneRenderSystem;

public:

	LightsRenderSystem();

	// Begin SceneRenderSystem overrides
	virtual void CollectRenderViews(const RenderScene& renderScene, const RenderView& mainRenderView, INOUT RenderViewsCollector& viewsCollector) override;
	virtual void RenderPerFrame(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, const lib::DynamicArray<ViewRenderingSpec*>& viewSpecs) override;
	// End SceneRenderSystem overrides

private:

	void RenderPerView(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec);

	void BuildLightsTiles(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& context);

	void CacheShadowMapsDS(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene);

	rdr::PipelineStateID m_buildLightsZClustersPipeline;
	rdr::PipelineStateID m_generateLightsDrawCommandsPipeline;
	
	rdr::PipelineStateID m_buildLightsTilesPipeline;

	lib::MTHandle<ShadowMapsDS> m_shadowMapsDS;
};

} // spt::rsc
