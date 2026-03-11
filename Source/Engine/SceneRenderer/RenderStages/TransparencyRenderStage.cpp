#include "TransparencyRenderStage.h"
#include "RenderGraphBuilder.h"
#include "RenderScene.h"
#include "SceneRenderSystems/StaticMeshes/StaticMeshesRenderSystem.h"
#include "View/RenderView.h"


namespace spt::rsc
{

REGISTER_RENDER_STAGE(ERenderStage::Transparency, TransparencyRenderStage);


namespace oit_abuffer
{

void Render(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec)
{
}

} // oit_abuffer


TransparencyRenderStage::TransparencyRenderStage()
{

}

void TransparencyRenderStage::OnRender(rg::RenderGraphBuilder& graphBuilder, SceneRendererInterface& rendererInterface, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& stageContext)
{
	SPT_PROFILER_FUNCTION();

	oit_abuffer::Render(graphBuilder, renderScene, viewSpec);
}

} // spt::rsc
