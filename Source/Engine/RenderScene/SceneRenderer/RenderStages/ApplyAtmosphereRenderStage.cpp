#include "ApplyAtmosphereRenderStage.h"

namespace spt::rsc
{
REGISTER_RENDER_STAGE(ERenderStage::ApplyAtmosphere, ApplyAtmosphereRenderStage);

ApplyAtmosphereRenderStage::ApplyAtmosphereRenderStage()
{ }

void ApplyAtmosphereRenderStage::OnRender(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& stageContext)
{
	SPT_PROFILER_FUNCTION();

	GetStageEntries(viewSpec).BroadcastOnRenderStage(graphBuilder, renderScene, viewSpec, stageContext);
}

} // spt::rsc
