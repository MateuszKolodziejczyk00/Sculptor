#pragma once

#include "RenderSystem.h"
#include "Pipelines/PipelineState.h"

namespace spt::rsc
{

class RENDER_SCENE_API StaticMeshesRenderSystem : public RenderSystem
{
protected:

	using Super = RenderSystem;

public:
	
	StaticMeshesRenderSystem();

	// Begin RenderSystem overrides
	virtual void RenderPerView(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec) override;
	// End RenderSystem overrides

private:

	void RenderMeshesPerView(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& context);

	rdr::PipelineStateID cullSubmeshesPipeline;
	rdr::PipelineStateID cullMeshletsPipeline;
};

} // spt::rsc
