#include "PreRenderingRenderStage.h"

namespace spt::rsc
{

REGISTER_RENDER_STAGE(ERenderStage::PreRendering, PreRenderingRenderStage);


PreRenderingRenderStage::PreRenderingRenderStage()
{ }

void PreRenderingRenderStage::OnRender(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& stageContext)
{
	SPT_PROFILER_FUNCTION();
	
	GetStageEntries(viewSpec).BroadcastOnRenderStage(graphBuilder, renderScene, viewSpec, stageContext);
}

} // spt::rsc
