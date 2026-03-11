#include "PreRenderingRenderStage.h"

namespace spt::rsc
{

REGISTER_RENDER_STAGE(ERenderStage::PreRendering, PreRenderingRenderStage);


PreRenderingRenderStage::PreRenderingRenderStage()
{ }

void PreRenderingRenderStage::OnRender(rg::RenderGraphBuilder& graphBuilder, SceneRendererInterface& rendererInterface, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& stageContext)
{
	SPT_PROFILER_FUNCTION();
	
	viewSpec.GetRenderViewEntry(ERenderViewEntry::PreRendering).Broadcast(graphBuilder, rendererInterface, renderScene, viewSpec, RenderViewEntryContext{});
}

} // spt::rsc
