#include "GlobalIlluminationRenderStage.h"

namespace spt::rsc
{

REGISTER_RENDER_STAGE(ERenderStage::GlobalIllumination, GlobalIlluminationRenderStage);

GlobalIlluminationRenderStage::GlobalIlluminationRenderStage()
{ }

void GlobalIlluminationRenderStage::OnRender(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& stageContext)
{
	SPT_PROFILER_FUNCTION();

	GetStageEntries(viewSpec).BroadcastOnRenderStage(graphBuilder, renderScene, viewSpec, stageContext);
}

} // spt::rsc
