#include "StaticMeshShadowMapRenderer.h"
#include "RenderScene.h"
#include "Shadows/ShadowMapsManagerSubsystem.h"
#include "Lights/LightTypes.h"
#include "RenderGraphBuilder.h"
#include "Transfers/UploadUtils.h"
#include "SceneRenderer/RenderStages/ShadowMapRenderStage.h"
#include "Shadows/ShadowsRenderingTypes.h"
#include "StaticMeshGeometry.h"
#include "Common/ShaderCompilationInput.h"
#include "GeometryManager.h"

namespace spt::rsc
{

StaticMeshShadowMapRenderer::StaticMeshShadowMapRenderer()
{
	{
		const rdr::ShaderID buildDrawCommandsShader = rdr::ResourcesManager::CreateShader("Sculptor/StaticMeshes/StaticMesh_BuildShadowMapDrawCommands.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "BuildDrawCommandsCS"));
		m_buildDrawCommandsPipeline = rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("StaticMesh_BuildShadowMapDrawCommands"), buildDrawCommandsShader);
	}

	{
		rdr::GraphicsPipelineShaders shaders;
		shaders.vertexShader = rdr::ResourcesManager::CreateShader("Sculptor/StaticMeshes/StaticMesh_RenderDepth.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Vertex, "StaticMesh_DepthVS"));
		shaders.fragmentShader = rdr::ResourcesManager::CreateShader("Sculptor/StaticMeshes/StaticMesh_RenderDepth.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Fragment, "StaticMesh_DepthFS"));

		rhi::GraphicsPipelineDefinition depthPrepassPipelineDef;
		depthPrepassPipelineDef.primitiveTopology = rhi::EPrimitiveTopology::TriangleList;
		depthPrepassPipelineDef.rasterizationDefinition.cullMode = rhi::ECullMode::None;
		depthPrepassPipelineDef.renderTargetsDefinition.depthRTDefinition.format = ShadowMapRenderStage::GetRenderedDepthFormat();
		m_shadowMapRenderingPipeline = rdr::ResourcesManager::CreateGfxPipeline(RENDERER_RESOURCE_NAME("StaticMesh_ShadowMapPipeline"), shaders, depthPrepassPipelineDef);
	}
}

void StaticMeshShadowMapRenderer::RenderPerFrame(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene)
{
	SPT_PROFILER_FUNCTION();

	m_pointLightBatches.clear();

	if (const lib::SharedPtr<ShadowMapsManagerSubsystem> shadowMapsManager = renderScene.GetSceneSubsystem<ShadowMapsManagerSubsystem>())
	{
		const RenderSceneRegistry& sceneRegistry = renderScene.GetRegistry();

		const lib::DynamicArray<RenderSceneEntity>& pointLightsToUpdate = shadowMapsManager->GetPointLightsWithShadowMapsToUpdate();

		m_pointLightBatches.reserve(pointLightsToUpdate.size());

		for (const RenderSceneEntity pointLight : pointLightsToUpdate)
		{
			const PointLightData& pointLightData = sceneRegistry.get<PointLightData>(pointLight);
			const PointLightShadowMapComponent& pointLightShadowMap = sceneRegistry.get<PointLightShadowMapComponent>(pointLight);

			const StaticMeshRenderSceneSubsystem& staticMeshPrimsSystem = renderScene.GetSceneSubsystemChecked<StaticMeshRenderSceneSubsystem>();
			const lib::DynamicArray<StaticMeshBatchDefinition> batchDefs = staticMeshPrimsSystem.BuildBatchesForPointLight(pointLightData, mat::EMaterialType::Opaque);

			const lib::DynamicArray<RenderView*> shadowMapViews = shadowMapsManager->GetPointLightShadowMapViews(pointLightShadowMap);
			
			lib::DynamicArray<SMShadowMapBatch>& pointLightBatches = m_pointLightBatches[pointLight];

			for(const StaticMeshBatchDefinition& batchDef : batchDefs)
			{
				const SMShadowMapBatch batch = CreateBatch(graphBuilder, renderScene, shadowMapViews, batchDef);
				BuildBatchDrawCommands(graphBuilder, batch);
				pointLightBatches.emplace_back(batch);
			}
		}
	}
}

void StaticMeshShadowMapRenderer::RenderPerView(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& context)
{
	SPT_PROFILER_FUNCTION();

	const RenderView& renderView = viewSpec.GetRenderView();

	const RenderSceneEntityHandle viewEntity = viewSpec.GetRenderView().GetViewEntity();
	ShadowMapViewComponent& viewShadowMapData = viewEntity.get<ShadowMapViewComponent>();

	const auto foundBatches = m_pointLightBatches.find(viewShadowMapData.owningLight);
	if (foundBatches == std::cend(m_pointLightBatches))
	{
		return;
	}

	for (const SMShadowMapBatch& batch : foundBatches->second)
	{
		SMIndirectShadowMapCommandsParameters drawParams;
		drawParams.batchDrawCommandsBuffer = batch.perFaceData[viewShadowMapData.faceIdx].drawCommandsBuffer;
		drawParams.batchDrawCommandsCountBuffer = batch.indirectDrawCountsBuffer;

		graphBuilder.AddSubpass(RG_DEBUG_NAME("Render Static Meshes Batch"),
								rg::BindDescriptorSets(batch.batchDS,
													   batch.perFaceData[viewShadowMapData.faceIdx].drawDS,
													   StaticMeshUnifiedData::Get().GetUnifiedDataDS(),
													   GeometryManager::Get().GetGeometryDSState(),
													   renderView.GetRenderViewDS()),
								std::tie(drawParams),
								[maxDrawCallsNum = batch.batchedSubmeshesNum, faceIdx = viewShadowMapData.faceIdx, drawParams, this](const lib::SharedRef<rdr::RenderContext>& renderContext, rdr::CommandRecorder& recorder)
								{
									recorder.BindGraphicsPipeline(m_shadowMapRenderingPipeline);

									const rdr::BufferView& drawsBufferView = drawParams.batchDrawCommandsBuffer->GetResource();
									const rdr::BufferView& drawCountBufferView = drawParams.batchDrawCommandsCountBuffer->GetResource();
									recorder.DrawIndirectCount(drawsBufferView, 0, sizeof(SMDepthOnlyDrawCallData), drawCountBufferView, faceIdx * sizeof(Uint32), maxDrawCallsNum);
								});
	}
}

SMShadowMapBatch StaticMeshShadowMapRenderer::CreateBatch(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, const lib::DynamicArray<RenderView*>& batchedViews, const StaticMeshBatchDefinition& batchDef) const
{
	SPT_PROFILER_FUNCTION();

	SMShadowMapBatch batch;

	// Create batch data and buffer

	batch.batchDS = batchDef.batchDS;
	batch.batchedSubmeshesNum = static_cast<Uint32>(batchDef.batchElements.size());

	// Create indirect draw counts buffer

	const Uint64 indirectDrawCountsBuffersSize = batchedViews.size() * sizeof(Uint32);
	rhi::BufferDefinition bufferDef(indirectDrawCountsBuffersSize, lib::Flags(rhi::EBufferUsage::Storage, rhi::EBufferUsage::Indirect, rhi::EBufferUsage::TransferDst));
	const rg::RGBufferViewHandle indirectDrawCountsBuffer= graphBuilder.CreateBufferView(RG_DEBUG_NAME("IndirectDrawCountsPerSide"), bufferDef, rhi::EMemoryUsage::GPUOnly);

	graphBuilder.FillBuffer(RG_DEBUG_NAME("Initialize IndirectDrawCountsBuffer"), indirectDrawCountsBuffer, 0, indirectDrawCountsBuffersSize, 0);

	batch.indirectDrawCountsBuffer = indirectDrawCountsBuffer;

	// Create draw commands buffers per shadow map face

	const Uint32 facesNum = static_cast<Uint32>(batchedViews.size());
	SPT_CHECK(facesNum > 0 && facesNum <= 6);

	const rhi::BufferDefinition viewsCullingDataBufferDef(sizeof(SceneViewCullingData) * static_cast<Uint64>(facesNum), rhi::EBufferUsage::Storage);
	const lib::SharedRef<rdr::Buffer> viewsCullingDataBuffer = rdr::ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME("ViewsCullingData"), viewsCullingDataBufferDef, rhi::EMemoryUsage::CPUToGPU);
	SceneViewCullingData* cullingDataBufferPtr = reinterpret_cast<SceneViewCullingData*>(viewsCullingDataBuffer->GetRHI().MapPtr());

	const rhi::BufferDefinition commandsBufferDef(sizeof(SMDepthOnlyDrawCallData) * batchDef.batchElements.size(), lib::Flags(rhi::EBufferUsage::Storage, rhi::EBufferUsage::Indirect));

	batch.perFaceData.reserve(batchedViews.size());

	for (SizeType faceIdx = 0; faceIdx < batchedViews.size(); ++faceIdx)
	{
		SMShadowMapBatch::PerFaceData faceData;

		const rg::RGBufferViewHandle drawCommandsBuffer = graphBuilder.CreateBufferView(RG_DEBUG_NAME("ShadowMapDrawCommandsBuffer"), commandsBufferDef, rhi::EMemoryUsage::GPUOnly);

		const lib::SharedRef<SMDepthOnlyDrawInstancesDS> drawDS = rdr::ResourcesManager::CreateDescriptorSetState<SMDepthOnlyDrawInstancesDS>(RENDERER_RESOURCE_NAME("SMShadowMapDrawInstancesDS"));
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

	const lib::SharedRef<SMShadowMapCullingDS> cullingDS = rdr::ResourcesManager::CreateDescriptorSetState<SMShadowMapCullingDS>(RENDERER_RESOURCE_NAME("SMShadowMapCullingDS"));
	cullingDS->u_shadowMapsData = lightShadowMapsData;
	cullingDS->u_viewsCullingData = viewsCullingDataBuffer->CreateFullView();
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
												 batch.cullingDS,
												 StaticMeshUnifiedData::Get().GetUnifiedDataDS()));
}

} // spt::rsc
