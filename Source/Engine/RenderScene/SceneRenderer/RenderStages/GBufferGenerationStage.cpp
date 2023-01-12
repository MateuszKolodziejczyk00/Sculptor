#include "GBufferGenerationStage.h"
#include "View/ViewRenderingSpec.h"
#include "SceneRenderer/SceneRendererTypes.h"

namespace spt::rsc
{

GBufferGenerationStage::GBufferGenerationStage()
{ }

void GBufferGenerationStage::Render(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec)
{
	SPT_PROFILER_FUNCTION();

	RenderStageEntries& viewStageEntries = viewSpec.GetRenderStageEntries(ERenderStage::GBufferGenerationStage);

	RenderStageExecutionContext stageContext(ERenderStage::GBufferGenerationStage);

	viewStageEntries.GetPreRenderStageDelegate().Broadcast(graphBuilder, renderScene, viewSpec, stageContext);

	SPT_MAYBE_UNUSED
	GBufferData& gBuffer = viewSpec.GetData().Create<GBufferData>();

	//graphBuilder.AddRenderPass()

	viewStageEntries.GetOnRenderStage().Broadcast(graphBuilder, renderScene, viewSpec, stageContext);
	
	viewStageEntries.GetPostRenderStage().Broadcast(graphBuilder, renderScene, viewSpec, stageContext);
}

} // spt::rsc
