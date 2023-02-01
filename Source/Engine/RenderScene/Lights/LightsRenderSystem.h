#pragma once

#include "RenderSystem.h"
#include "Pipelines/PipelineState.h"


namespace spt::rsc
{

class RENDER_SCENE_API LightsRenderSystem : public RenderSystem
{
protected:

	using Super = RenderSystem;

public:

	LightsRenderSystem();

	// Begin RenderSystem overrides
	virtual void RenderPerView(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec) override;
	// End RenderSystem overrides

private:

	rdr::PipelineStateID m_buildLightsZClustersPipeline;
};

} // spt::rsc
