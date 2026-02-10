#include "TransparencyRenderStage.h"
#include "RenderGraphBuilder.h"
#include "RenderScene.h"
#include "StaticMeshes/StaticMeshesRenderSystem.h"
#include "View/RenderView.h"


namespace spt::rsc
{

REGISTER_RENDER_STAGE(ERenderStage::Transparency, TransparencyRenderStage);


namespace oit_abuffer
{

void Render(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec)
{
	SPT_PROFILER_FUNCTION();

	const math::Vector2u transparencyRes = viewSpec.GetRenderingRes();

	const StaticMeshesRenderSystem& staticMeshesSystem       = renderScene.GetRenderSystemChecked<StaticMeshesRenderSystem>();
	const GeometryPassDataCollection& cachedGeometryPassData = staticMeshesSystem.GetCachedTransparentGeometryPassData();

	if (cachedGeometryPassData.materialBatches.empty())
	{
		return;
	}

	SPT_CHECK(cachedGeometryPassData.materialBatches.size() == 1u);

	//const rg::RGTextureViewHandle layersTexture = graphBuilder.CreateTextureView(RG_DEBUG_NAME("Transparency Layers Num"), rg::TextureDef(transparencyRes, rhi::EFragmentFormat::R32_U_Int));

	//graphBuilder.ClearTexture(RG_DEBUG_NAME("Clear Transparency Layers Texture"), layersTexture, rhi::ClearColor(0u, 0u, 0u, 0u));


}

} // oit_abuffer


TransparencyRenderStage::TransparencyRenderStage()
{

}

void TransparencyRenderStage::OnRender(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& stageContext)
{
	SPT_PROFILER_FUNCTION();

	oit_abuffer::Render(graphBuilder, renderScene, viewSpec);

	GetStageEntries(viewSpec).BroadcastOnRenderStage(graphBuilder, renderScene, viewSpec, stageContext);
}

} // spt::rsc
