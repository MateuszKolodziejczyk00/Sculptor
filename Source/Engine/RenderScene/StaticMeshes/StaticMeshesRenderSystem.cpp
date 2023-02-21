#include "StaticMeshesRenderSystem.h"

namespace spt::rsc
{

StaticMeshesRenderSystem::StaticMeshesRenderSystem()
{
	m_supportedStages = lib::Flags(ERenderStage::ForwardOpaque, ERenderStage::DepthPrepass, ERenderStage::ShadowMap);
}

void StaticMeshesRenderSystem::RenderPerFrame(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene)
{
	Super::RenderPerFrame(graphBuilder, renderScene);

	m_shadowMapRenderer.RenderPerFrame(graphBuilder, renderScene);
}

void StaticMeshesRenderSystem::RenderPerView(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec)
{
	SPT_PROFILER_FUNCTION();

	Super::RenderPerView(graphBuilder, renderScene, viewSpec);

	const RenderView& renderView = viewSpec.GetRenderView();

	SMViewRenderingParameters viewRenderingParams;
	viewRenderingParams.rtResolution = viewSpec.GetRenderView().GetRenderingResolution();

	const lib::SharedRef<SMProcessBatchForViewDS> viewDS = rdr::ResourcesManager::CreateDescriptorSetState<SMProcessBatchForViewDS>(RENDERER_RESOURCE_NAME("SMViewDS"));
	viewDS->u_sceneView				= renderView.GetViewRenderingData();
	viewDS->u_cullingData			= renderView.GetCullingData();
	viewDS->u_viewRenderingParams	= viewRenderingParams;

	viewSpec.GetData().Create<SMRenderingViewData>(SMRenderingViewData{ std::move(viewDS) });
	
	if (viewSpec.SupportsStage(ERenderStage::ShadowMap))
	{
		RenderStageEntries& shadowMapStageEntries = viewSpec.GetRenderStageEntries(ERenderStage::ShadowMap);
		shadowMapStageEntries.GetOnRenderStage().AddRawMember(&m_shadowMapRenderer, &StaticMeshShadowMapRenderer::RenderPerView);
	}
	
	if (viewSpec.SupportsStage(ERenderStage::DepthPrepass))
	{
		const Bool hasAnyBatches = m_depthPrepassRenderer.BuildBatchesPerView(graphBuilder, renderScene, viewSpec);

		if (hasAnyBatches)
		{
			m_depthPrepassRenderer.CullPerView(graphBuilder, renderScene, viewSpec);

			RenderStageEntries& depthPrepassStageEntries = viewSpec.GetRenderStageEntries(ERenderStage::DepthPrepass);
			depthPrepassStageEntries.GetOnRenderStage().AddRawMember(&m_depthPrepassRenderer, &StaticMeshDepthPrepassRenderer::RenderPerView);
		}
	}

	if (viewSpec.SupportsStage(ERenderStage::ForwardOpaque))
	{
		const Bool hasAnyBatches = m_forwardOpaqueRenderer.BuildBatchesPerView(graphBuilder, renderScene, viewSpec);

		if (hasAnyBatches)
		{
			RenderStageEntries& depthPrepassStageEntries = viewSpec.GetRenderStageEntries(ERenderStage::DepthPrepass);
			depthPrepassStageEntries.GetPostRenderStage().AddRawMember(&m_forwardOpaqueRenderer, &StaticMeshForwardOpaqueRenderer::CullPerView);

			RenderStageEntries& basePassStageEntries = viewSpec.GetRenderStageEntries(ERenderStage::ForwardOpaque);
			basePassStageEntries.GetOnRenderStage().AddRawMember(&m_forwardOpaqueRenderer, &StaticMeshForwardOpaqueRenderer::RenderPerView);
		}
	}
}

} // spt::rsc
