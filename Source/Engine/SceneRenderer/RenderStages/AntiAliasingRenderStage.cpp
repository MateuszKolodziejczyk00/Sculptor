#include "AntiAliasingRenderStage.h"

namespace spt::rsc
{

REGISTER_RENDER_STAGE(ERenderStage::AntiAliasing, AntiAliasingRenderStage);


AntiAliasingRenderStage::AntiAliasingRenderStage()
{ }

void AntiAliasingRenderStage::OnRender(rg::RenderGraphBuilder& graphBuilder, SceneRendererInterface& rendererInterface, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& stageContext)
{
	SPT_PROFILER_FUNCTION();
	
	viewSpec.GetRenderViewEntry(ERenderViewEntry::AntiAliasing).Broadcast(graphBuilder, rendererInterface, renderScene, viewSpec, RenderViewEntryContext{});
}

} // spt::rsc
