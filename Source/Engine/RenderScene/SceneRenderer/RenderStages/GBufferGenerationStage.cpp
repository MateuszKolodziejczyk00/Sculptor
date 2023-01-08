#include "GBufferGenerationStage.h"
#include "View/ViewRenderingSpec.h"
#include "SceneRenderer/SceneRendererTypes.h"

namespace spt::rsc
{

GBufferGenerationStage::GBufferGenerationStage()
{ }

void GBufferGenerationStage::Render(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& view)
{
	SPT_PROFILER_FUNCTION();

	RenderStageEntries& viewStageEntries = view.GetRenderStageEntries(ERenderStage::GBufferGenerationStage);

	viewStageEntries.GetPreRenderStageDelegate().Broadcast(graphBuilder, renderScene, view);

	SPT_MAYBE_UNUSED
	GBufferData& gBuffer = view.GetData().Create<GBufferData>();

	//graphBuilder.AddRenderPass()

	viewStageEntries.GetOnRenderStage().Broadcast(graphBuilder, renderScene, view);
	
	viewStageEntries.GetPostRenderStage().Broadcast(graphBuilder, renderScene, view);
}

} // spt::rsc
