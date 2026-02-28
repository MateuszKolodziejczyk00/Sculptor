#include "StaticMeshShadowMapRenderer.h"
#include "Lights/LightTypes.h"
#include "RenderScene.h"
#include "Shadows/ShadowMapsRenderSystem.h"
#include "RenderGraphBuilder.h"
#include "StaticMeshes/StaticMeshesRenderSystem.h"
#include "Utils/TextureUtils.h"
#include "SceneRenderer/RenderStages/ShadowMapRenderStage.h"
#include "Shadows/ShadowsRenderingTypes.h"
#include "Common/ShaderCompilationInput.h"

namespace spt::rsc
{

GRAPHICS_PSO(SMDepthOnlyPSO)
{
	VERTEX_SHADER("Sculptor/StaticMeshes/SMDepthOnly.hlsl", SMDepthOnly_VS);
	FRAGMENT_SHADER("Sculptor/StaticMeshes/SMDepthOnly.hlsl", SMDepthOnly_FS);

	PERMUTATION_DOMAIN(SMDepthOnlyPermutation);
};

static rdr::PipelineStateID GetPipelineStateForBatch(const SMShadowMapBatch& batch)
{
	const rhi::GraphicsPipelineDefinition depthOnlyPipelineDef
	{
		.primitiveTopology = rhi::EPrimitiveTopology::TriangleList,
		.rasterizationDefinition =
		{
			.cullMode = rhi::ECullMode::None,
		},
		.renderTargetsDefinition =
		{
			.depthRTDefinition = rhi::DepthRenderTargetDefinition
			{
				.format = ShadowMapRenderStage::GetRenderedDepthFormat(),
			}
		}
	};

	return SMDepthOnlyPSO::GetPermutation(depthOnlyPipelineDef, batch.permutation);
}

StaticMeshShadowMapRenderer::StaticMeshShadowMapRenderer()
{
	{
		const rdr::ShaderID buildDrawCommandsShader = rdr::ResourcesManager::CreateShader("Sculptor/StaticMeshes/StaticMesh_BuildShadowMapDrawCommands.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "BuildDrawCommandsCS"));
		m_buildDrawCommandsPipeline = rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("StaticMesh_BuildShadowMapDrawCommands"), buildDrawCommandsShader);
	}
}

void StaticMeshShadowMapRenderer::RenderPerFrame(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, const lib::DynamicArray<ViewRenderingSpec*>& viewSpecs)
{
	SPT_PROFILER_FUNCTION();

	m_pointLightBatches.clear();
	m_globalShadowViewBatches.clear();

	if (const lib::SharedPtr<ShadowMapsRenderSystem> shadowMapsRenderSystem = renderScene.FindRenderSystem<ShadowMapsRenderSystem>())
	{
		const RenderSceneRegistry& sceneRegistry = renderScene.GetRegistry();

		const lib::DynamicArray<RenderSceneEntity>& pointLightsToUpdate = shadowMapsRenderSystem->GetPointLightsWithShadowMapsToUpdate();

		m_pointLightBatches.reserve(pointLightsToUpdate.size());

		for (const RenderSceneEntity pointLight : pointLightsToUpdate)
		{
			const PointLightData& pointLightData = sceneRegistry.get<PointLightData>(pointLight);
			const PointLightShadowMapComponent& pointLightShadowMap = sceneRegistry.get<PointLightShadowMapComponent>(pointLight);

			const StaticMeshesRenderSystem& staticMeshesSystem = renderScene.GetRenderSystemChecked<StaticMeshesRenderSystem>();
			const lib::DynamicArray<StaticMeshSMBatchDefinition> batchDefs = staticMeshesSystem.BuildBatchesForPointLightSM(pointLightData);

			const lib::DynamicArray<RenderView*> shadowMapViews = shadowMapsRenderSystem->GetPointLightShadowMapViews(pointLightShadowMap);
			
			lib::DynamicArray<SMShadowMapBatch>& pointLightBatches = m_pointLightBatches[pointLight];

			for(const StaticMeshSMBatchDefinition& batchDef : batchDefs)
			{
				const SMShadowMapBatch batch = CreateBatch(graphBuilder, renderScene, shadowMapViews, batchDef);
				BuildBatchDrawCommands(graphBuilder, batch);
				pointLightBatches.emplace_back(batch);
			}
		}
	}

	for (ViewRenderingSpec* viewSpec : viewSpecs)
	{
		const rg::BindDescriptorSetsScope rendererDSScope(graphBuilder, rg::BindDescriptorSets(viewSpec->GetRenderView().GetRenderViewDS()));

		RenderView& renderView = viewSpec->GetRenderView();
		const ShadowMapViewComponent& viewShadowMapData = renderView.GetBlackboard().Get<ShadowMapViewComponent>();

		if (viewShadowMapData.shadowMapType == EShadowMapType::DirectionalLightCascade)
		{
			const StaticMeshesRenderSystem& staticMeshesSystem = renderScene.GetRenderSystemChecked<StaticMeshesRenderSystem>();
			const lib::DynamicArray<StaticMeshSMBatchDefinition>& batchDefs = staticMeshesSystem.BuildBatchesForSMView(renderView);

			lib::DynamicArray<SMShadowMapBatch>& viewBatches = m_globalShadowViewBatches[&renderView];

			for (const StaticMeshSMBatchDefinition& batchDef : batchDefs)
			{
				lib::DynamicArray<RenderView*> shadowMapViews = { &renderView };
				const SMShadowMapBatch batch = CreateBatch(graphBuilder, renderScene, shadowMapViews, batchDef);
				BuildBatchDrawCommands(graphBuilder, batch);
				viewBatches.emplace_back(batch);
			}
		}
	}
}

void StaticMeshShadowMapRenderer::RenderToShadowMap(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& context, RenderStageContextMetaDataHandle metaData)
{
	SPT_PROFILER_FUNCTION();

	RenderView& renderView = viewSpec.GetRenderView();

	ShadowMapViewComponent& viewShadowMapData = renderView.GetBlackboard().Get<ShadowMapViewComponent>();

	lib::DynamicArray<SMShadowMapBatch>* batches = nullptr;
	if (viewShadowMapData.shadowMapType == EShadowMapType::DirectionalLightCascade)
	{
		batches = &m_globalShadowViewBatches.at(&renderView);
	}
	else if(viewShadowMapData.shadowMapType == EShadowMapType::PointLightFace)
	{
		batches = &m_pointLightBatches.at(viewShadowMapData.owningLight);
	}

	for (const SMShadowMapBatch& batch : *batches)
	{
		SMIndirectShadowMapCommandsParameters drawParams;
		drawParams.batchDrawCommandsBuffer = batch.perFaceData[viewShadowMapData.faceIdx].drawCommandsBuffer;
		drawParams.batchDrawCommandsCountBuffer = batch.indirectDrawCountsBuffer;

		graphBuilder.AddSubpass(RG_DEBUG_NAME("Render Static Meshes Batch"),
								rg::BindDescriptorSets(batch.batchDS,
													   batch.perFaceData[viewShadowMapData.faceIdx].drawDS),
								std::tie(drawParams),
								[maxDrawCallsNum = batch.batchedSubmeshesNum, faceIdx = viewShadowMapData.faceIdx, pipelineID = GetPipelineStateForBatch(batch), drawParams, this](const lib::SharedRef<rdr::RenderContext>& renderContext, rdr::CommandRecorder& recorder)
								{
									recorder.BindGraphicsPipeline(pipelineID);

									const rdr::BufferView& drawsBufferView     = *drawParams.batchDrawCommandsBuffer->GetResource();
									const rdr::BufferView& drawCountBufferView = *drawParams.batchDrawCommandsCountBuffer->GetResource();
									recorder.DrawIndirectCount(drawsBufferView, 0, sizeof(SMDepthOnlyDrawCallData), drawCountBufferView, faceIdx * sizeof(Uint32), maxDrawCallsNum);
								});
	}
}

void StaticMeshShadowMapRenderer::FinishRenderingFrame()
{
	m_globalShadowViewBatches.clear();
	m_pointLightBatches.clear();
}

SMShadowMapBatch StaticMeshShadowMapRenderer::CreateBatch(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, const lib::DynamicArray<RenderView*>& batchedViews, const StaticMeshSMBatchDefinition& batchDef) const
{
	SPT_PROFILER_FUNCTION();

	SMShadowMapBatch batch;

	// Create batch data and buffer

	batch.batchDS             = batchDef.batchDS;
	batch.batchedSubmeshesNum = batchDef.batchElementsNum;
	batch.permutation         = batchDef.permutation;

	// Create indirect draw counts buffer

	const Uint64 indirectDrawCountsBuffersSize = batchedViews.size() * sizeof(Uint32);
	rhi::BufferDefinition bufferDef(indirectDrawCountsBuffersSize, lib::Flags(rhi::EBufferUsage::Storage, rhi::EBufferUsage::Indirect, rhi::EBufferUsage::TransferDst));
	const rg::RGBufferViewHandle indirectDrawCountsBuffer = graphBuilder.CreateBufferView(RG_DEBUG_NAME("IndirectDrawCountsPerSide"), bufferDef, rhi::EMemoryUsage::GPUOnly);

	graphBuilder.FillBuffer(RG_DEBUG_NAME("Initialize IndirectDrawCountsBuffer"), indirectDrawCountsBuffer, 0, indirectDrawCountsBuffersSize, 0);

	batch.indirectDrawCountsBuffer = indirectDrawCountsBuffer;

	// Create draw commands buffers per shadow map face

	const Uint32 facesNum = static_cast<Uint32>(batchedViews.size());
	SPT_CHECK(facesNum > 0 && facesNum <= 6);

	const rhi::BufferDefinition viewsCullingDataBufferDef(sizeof(SceneViewCullingData) * static_cast<Uint64>(facesNum), rhi::EBufferUsage::Storage);
	const lib::SharedRef<rdr::Buffer> viewsCullingDataBuffer = rdr::ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME("ViewsCullingData"), viewsCullingDataBufferDef, rhi::EMemoryUsage::CPUToGPU);
	SceneViewCullingData* cullingDataBufferPtr = reinterpret_cast<SceneViewCullingData*>(viewsCullingDataBuffer->GetRHI().MapPtr());

	const rhi::BufferDefinition commandsBufferDef(sizeof(SMDepthOnlyDrawCallData) * batchDef.batchElementsNum, lib::Flags(rhi::EBufferUsage::Storage, rhi::EBufferUsage::Indirect));

	batch.perFaceData.reserve(batchedViews.size());

	for (SizeType faceIdx = 0; faceIdx < batchedViews.size(); ++faceIdx)
	{
		SMShadowMapBatch::PerFaceData faceData;

		const rg::RGBufferViewHandle drawCommandsBuffer = graphBuilder.CreateBufferView(RG_DEBUG_NAME("ShadowMapDrawCommandsBuffer"), commandsBufferDef, rhi::EMemoryUsage::GPUOnly);

		const lib::MTHandle<SMDepthOnlyDrawInstancesDS> drawDS = graphBuilder.CreateDescriptorSet<SMDepthOnlyDrawInstancesDS>(RENDERER_RESOURCE_NAME("SMShadowMapDrawInstancesDS"));
		drawDS->u_drawCommands = drawCommandsBuffer;

		faceData.drawDS	= drawDS;
		faceData.drawCommandsBuffer = drawCommandsBuffer;

		batch.perFaceData.emplace_back(faceData);

		cullingDataBufferPtr[faceIdx] = batchedViews[faceIdx]->GetCullingData();
	}

	viewsCullingDataBuffer->GetRHI().Unmap();

	// Create culling resources

	SMLightShadowMapsData lightShadowMapsData;
	lightShadowMapsData.facesNum = facesNum;

	const lib::MTHandle<SMShadowMapCullingDS> cullingDS = graphBuilder.CreateDescriptorSet<SMShadowMapCullingDS>(RENDERER_RESOURCE_NAME("SMShadowMapCullingDS"));
	cullingDS->u_shadowMapsData = lightShadowMapsData;
	cullingDS->u_viewsCullingData = viewsCullingDataBuffer->GetFullView();
	cullingDS->u_drawsCount = indirectDrawCountsBuffer;
	cullingDS->u_drawCommandsFace0 = batch.perFaceData[0].drawCommandsBuffer;
	if (facesNum > 1)
	{
		cullingDS->u_drawCommandsFace1 = batch.perFaceData[1].drawCommandsBuffer;
	}
	if (facesNum > 2)
	{
		cullingDS->u_drawCommandsFace2 = batch.perFaceData[2].drawCommandsBuffer;
	}
	if (facesNum > 3)
	{
		cullingDS->u_drawCommandsFace3 = batch.perFaceData[3].drawCommandsBuffer;
	}
	if (facesNum > 4)
	{
		cullingDS->u_drawCommandsFace4 = batch.perFaceData[4].drawCommandsBuffer;
	}
	if (facesNum > 1)
	{
		cullingDS->u_drawCommandsFace5 = batch.perFaceData[5].drawCommandsBuffer;
	}

	batch.cullingDS = cullingDS;

	return batch;
}

void StaticMeshShadowMapRenderer::BuildBatchDrawCommands(rg::RenderGraphBuilder& graphBuilder, const SMShadowMapBatch& batch)
{
	SPT_PROFILER_FUNCTION();

	const Uint32 groupsCount = math::Utils::DivideCeil(batch.batchedSubmeshesNum, 64u);

	graphBuilder.Dispatch(RG_DEBUG_NAME("Build Shadow Maps SM Draw Commands"),
						  m_buildDrawCommandsPipeline,
						  math::Vector3u(groupsCount, 1, 1),
						  rg::BindDescriptorSets(batch.batchDS,
												 batch.cullingDS));
}

} // spt::rsc
