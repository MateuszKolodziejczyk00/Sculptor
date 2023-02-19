#include "StaticMeshesRenderSystem.h"

namespace spt::rsc
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// Shadow Map ====================================================================================

BEGIN_RG_NODE_PARAMETERS_STRUCT(SMIndirectShadowMapCommandsParameters)
	RG_BUFFER_VIEW(batchDrawCommandsBuffer, rg::ERGBufferAccess::Read, rhi::EPipelineStage::DrawIndirect)
	RG_BUFFER_VIEW(batchDrawCommandsCountBuffer, rg::ERGBufferAccess::Read, rhi::EPipelineStage::DrawIndirect)
END_RG_NODE_PARAMETERS_STRUCT();


struct SMShadowMapBatch
{
	lib::SharedPtr<StaticMeshBatchDS>	batchDS;

	lib::DynamicArray<rg::RGBufferViewHandle> drawCommandsBuffersPerSide;
	lib::DynamicArray<rg::RGBufferViewHandle> indirectDrawCountBuffersPerSide;

};

//////////////////////////////////////////////////////////////////////////////////////////////////
// StaticMeshesRenderSystem ======================================================================

StaticMeshesRenderSystem::StaticMeshesRenderSystem()
{
	m_supportedStages = lib::Flags(ERenderStage::ForwardOpaque, ERenderStage::DepthPrepass, ERenderStage::ShadowMap);
}

void StaticMeshesRenderSystem::RenderPerFrame(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene)
{
	SPT_PROFILER_FUNCTION();

	Super::RenderPerFrame(graphBuilder, renderScene);

	//if (const ShadowMapsManagerSystem* shadowMapsManager = renderScene.GetPrimitivesSystem<ShadowMapsManagerSystem>())
	//{
	//	const RenderSceneRegistry& sceneRegistry = renderScene.GetRegistry();

	//	const lib::DynamicArray<RenderSceneEntity>& pointLightsToUpdate = shadowMapsManager->GetPointLightsWithShadowMapsToUpdate();

	//	for (const RenderSceneEntity pointLight : pointLightsToUpdate)
	//	{
	//		const PointLightData& pointLightData = sceneRegistry.get<PointLightData>(pointLight);

	//		const StaticMeshPrimitivesSystem& staticMeshPrimsSystem = renderScene.GetPrimitivesSystemChecked<StaticMeshPrimitivesSystem>();
	//		SPT_MAYBE_UNUSED
	//		const StaticMeshBatchDefinition batchDef = staticMeshPrimsSystem.BuildBatchForPointLight(pointLightData);
	//		
	//		// TODO...
	//	}
	//}
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
