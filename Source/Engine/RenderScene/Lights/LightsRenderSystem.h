#pragma once

#include "RenderSystem.h"
#include "Pipelines/PipelineTypes.h"


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


class RENDER_SCENE_API LightsRenderSystem : public RenderSystem
{
protected:

	using Super = RenderSystem;

public:

	LightsRenderSystem();

	// Begin RenderSystem overrides
	virtual void CollectRenderViews(const RenderScene& renderScene, const RenderView& mainRenderView, INOUT lib::DynamicArray<RenderView*>& outViews) override;
	virtual void RenderPerFrame(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene) override;
	virtual void RenderPerView(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec) override;
	// End RenderSystem overrides

private:

	void BuildLightsTiles(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& context);

	void CacheShadowMapsDS(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene);

	rdr::PipelineStateID m_buildLightsZClustersPipeline;
	rdr::PipelineStateID m_generateLightsDrawCommandsPipeline;
	
	rdr::PipelineStateID m_buildLightsTilesPipeline;

	lib::SharedPtr<ShadowMapsDS> m_shadowMapsDS;
};

} // spt::rsc
