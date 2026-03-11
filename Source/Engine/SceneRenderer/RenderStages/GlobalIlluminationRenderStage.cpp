#include "GlobalIlluminationRenderStage.h"

namespace spt::rsc
{

REGISTER_RENDER_STAGE(ERenderStage::GlobalIllumination, GlobalIlluminationRenderStage);

GlobalIlluminationRenderStage::GlobalIlluminationRenderStage()
{ }

void GlobalIlluminationRenderStage::OnRender(rg::RenderGraphBuilder& graphBuilder, SceneRendererInterface& rendererInterface, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& stageContext)
{
	SPT_PROFILER_FUNCTION();

	viewSpec.GetRenderViewEntry(ERenderViewEntry::CreateGlobalLightsDS).Broadcast(graphBuilder, rendererInterface, renderScene, viewSpec, RenderViewEntryContext{});
	viewSpec.GetRenderViewEntry(ERenderViewEntry::RenderGI).Broadcast(graphBuilder, rendererInterface, renderScene, viewSpec, RenderViewEntryContext{});
}

} // spt::rsc
