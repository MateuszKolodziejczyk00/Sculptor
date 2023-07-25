#include "ApplyAtmosphereRenderStage.h"

namespace spt::rsc
{

ApplyAtmosphereRenderStage::ApplyAtmosphereRenderStage()
{ }

void ApplyAtmosphereRenderStage::OnRender(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& stageContext)
{
	SPT_PROFILER_FUNCTION();

	GetStageEntries(viewSpec).GetOnRenderStage().Broadcast(graphBuilder, renderScene, viewSpec, stageContext);
}

} // spt::rsc
