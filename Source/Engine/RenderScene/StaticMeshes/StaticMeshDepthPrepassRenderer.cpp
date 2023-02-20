#include "StaticMeshDepthPrepassRenderer.h"
#include "Common/ShaderCompilationInput.h"
#include "ResourcesManager.h"
#include "View/ViewRenderingSpec.h"
#include "RenderGraphBuilder.h"
#include "GeometryManager.h"
#include "StaticMeshGeometry.h"
#include "SceneRenderer/RenderStages/DepthPrepassRenderStage.h"

namespace spt::rsc
{

StaticMeshDepthPrepassRenderer::StaticMeshDepthPrepassRenderer()
{
	{
		sc::ShaderCompilationSettings compilationSettings;
		compilationSettings.AddShaderToCompile(sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "BuildDrawCommandsCS"));
		const rdr::ShaderID buildDrawCommandsShader = rdr::ResourcesManager::CreateShader("Sculptor/StaticMeshes/StaticMesh_BuildDepthPrepassDrawCommands.hlsl", compilationSettings);
		m_buildDrawCommandsPipeline = rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("StaticMesh_BuildDepthPrepassDrawCommands"), buildDrawCommandsShader);
	}

	{
		sc::ShaderCompilationSettings compilationSettings;
		compilationSettings.AddShaderToCompile(sc::ShaderStageCompilationDef(rhi::EShaderStage::Vertex, "StaticMesh_DepthVS"));
		compilationSettings.AddShaderToCompile(sc::ShaderStageCompilationDef(rhi::EShaderStage::Fragment, "StaticMesh_DepthFS"));
		const rdr::ShaderID depthShader = rdr::ResourcesManager::CreateShader("Sculptor/StaticMeshes/StaticMesh_RenderDepth.hlsl", compilationSettings);

		rhi::GraphicsPipelineDefinition depthPrepassPipelineDef;
		depthPrepassPipelineDef.primitiveTopology = rhi::EPrimitiveTopology::TriangleList;
		depthPrepassPipelineDef.renderTargetsDefinition.depthRTDefinition.format = DepthPrepassRenderStage::GetDepthFormat();
		m_depthPrepassRenderingPipeline = rdr::ResourcesManager::CreateGfxPipeline(RENDERER_RESOURCE_NAME("StaticMesh_DepthPrepassPipeline"), depthPrepassPipelineDef, depthShader);
	}
}

Bool StaticMeshDepthPrepassRenderer::BuildBatchesPerView(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec)
{
	SPT_PROFILER_FUNCTION();
	
	const RenderView& renderView = viewSpec.GetRenderView();
	RenderSceneEntityHandle viewEntityHandle = renderView.GetViewEntity();
	
	// Get instances visible last frame
	const StaticMeshesVisibleLastFrame* visibleInstances = viewEntityHandle.try_get<StaticMeshesVisibleLastFrame>();
	if (visibleInstances)
	{
		lib::DynamicArray<SMDepthPrepassBatch> batches;

		for (const StaticMeshesVisibleLastFrame::BatchElementsInfo& instances : visibleInstances->visibleInstances)
		{
			batches.emplace_back(CreateBatch(graphBuilder, renderScene, instances));
		}

		if (!batches.empty())
		{
			SMDepthPrepassBatches& viewBatches = viewSpec.GetData().Create<SMDepthPrepassBatches>();
			viewBatches.batches = std::move(batches);

			return true;
		}
	}

	return false;
}

void StaticMeshDepthPrepassRenderer::CullPerView(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec)
{
	SPT_PROFILER_FUNCTION();

	const SMDepthPrepassBatches& depthPrepassBatches = viewSpec.GetData().Get<SMDepthPrepassBatches>();

	const SMRenderingViewData& staticMeshRenderingViewData = viewSpec.GetData().Get<SMRenderingViewData>();

	const rg::BindDescriptorSetsScope staticMeshCullingDSScope(graphBuilder,
															   rg::BindDescriptorSets(lib::Ref(StaticMeshUnifiedData::Get().GetUnifiedDataDS()),
																					  lib::Ref(staticMeshRenderingViewData.viewDS)));

	for (const SMDepthPrepassBatch& batch : depthPrepassBatches.batches)
	{
		const rg::BindDescriptorSetsScope staticMeshBatchDSScope(graphBuilder, rg::BindDescriptorSets(lib::Ref(batch.batchDS)));

		const Uint32 dispatchGroupsNum = math::Utils::RoundUp<Uint32>(batch.batchedSubmeshesNum, 64) / 64;

		graphBuilder.Dispatch(RG_DEBUG_NAME("SM Cull And Build Draw Commands"),
							  m_buildDrawCommandsPipeline,
							  math::Vector3u(dispatchGroupsNum, 1, 1),
							  rg::BindDescriptorSets(lib::Ref(batch.cullInstancesDS)));
	}
}

void StaticMeshDepthPrepassRenderer::RenderPerView(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& context)
{
	SPT_PROFILER_FUNCTION();

	const SMDepthPrepassBatches& depthPrepassBatches = viewSpec.GetData().Get<SMDepthPrepassBatches>();

	const SMRenderingViewData& staticMeshRenderingViewData = viewSpec.GetData().Get<SMRenderingViewData>();

	const rg::BindDescriptorSetsScope staticMeshRenderingDSScope(graphBuilder,
																 rg::BindDescriptorSets(lib::Ref(StaticMeshUnifiedData::Get().GetUnifiedDataDS()),
																						lib::Ref(GeometryManager::Get().GetGeometryDSState()),
																						lib::Ref(staticMeshRenderingViewData.viewDS)));

	for (const SMDepthPrepassBatch& batch : depthPrepassBatches.batches)
	{
		const rg::BindDescriptorSetsScope staticMeshBatchDSScope(graphBuilder, rg::BindDescriptorSets(lib::Ref(batch.batchDS)));

		SMIndirectDepthPrepassCommandsParameters drawParams;
		drawParams.batchDrawCommandsBuffer = batch.drawCommandsBuffer;
		drawParams.batchDrawCommandsCountBuffer = batch.indirectDrawCountBuffer;

		const lib::SharedRef<GeometryDS> unifiedGeometryDS = lib::Ref(GeometryManager::Get().GetGeometryDSState());

		const Uint32 maxDrawCallsNum = batch.batchedSubmeshesNum;

		graphBuilder.AddSubpass(RG_DEBUG_NAME("Render Static Meshes Batch"),
								rg::BindDescriptorSets(lib::Ref(batch.drawInstancesDS)),
								std::tie(drawParams),
								[maxDrawCallsNum, drawParams, this](const lib::SharedRef<rdr::RenderContext>& renderContext, rdr::CommandRecorder& recorder)
								{
									recorder.BindGraphicsPipeline(m_depthPrepassRenderingPipeline);

									const rdr::BufferView& drawsBufferView = drawParams.batchDrawCommandsBuffer->GetResource();
									const rdr::BufferView& drawCountBufferView = drawParams.batchDrawCommandsCountBuffer->GetResource();
									recorder.DrawIndirectCount(drawsBufferView, 0, sizeof(SMDepthOnlyDrawCallData), drawCountBufferView, 0, maxDrawCallsNum);
								});
	}
}

SMDepthPrepassBatch StaticMeshDepthPrepassRenderer::CreateBatch(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, const StaticMeshesVisibleLastFrame::BatchElementsInfo& batchElements) const
{
	SPT_PROFILER_FUNCTION();

	SMDepthPrepassBatch batch;

	// Create batch info
	
	SMGPUBatchData batchData;
	batchData.elementsNum = batchElements.batchedSubmeshesNum;

	const lib::SharedRef<StaticMeshBatchDS> batchDS = rdr::ResourcesManager::CreateDescriptorSetState<StaticMeshBatchDS>(RENDERER_RESOURCE_NAME("DepthPrepassBatchDS"));
	batchDS->u_batchElements = batchElements.visibleBatchElementsBuffer->CreateFullView();
	batchDS->u_batchData = batchData;

	batch.batchDS = batchDS;
	batch.batchedSubmeshesNum = batchElements.batchedSubmeshesNum;

	// Create buffers

	const rhi::RHIAllocationInfo batchBuffersAllocInfo(rhi::EMemoryUsage::GPUOnly);

	const rhi::BufferDefinition commandsBufferDef(sizeof(SMDepthOnlyDrawCallData) * batchElements.batchedSubmeshesNum, lib::Flags(rhi::EBufferUsage::Storage, rhi::EBufferUsage::Indirect));
	const rg::RGBufferViewHandle drawCommandsBuffer = graphBuilder.CreateBufferView(RG_DEBUG_NAME("SMDepthPrepassIndirectCommands"), commandsBufferDef, batchBuffersAllocInfo);

	const Uint64 indirectDrawsCountSize = sizeof(Uint32);
	const rhi::BufferDefinition indirectDrawCountBufferDef(indirectDrawsCountSize, lib::Flags(rhi::EBufferUsage::Storage, rhi::EBufferUsage::Indirect, rhi::EBufferUsage::TransferDst));
	const rg::RGBufferViewHandle indirectDrawCountBuffer = graphBuilder.CreateBufferView(RG_DEBUG_NAME("SMDepthPrepassIndirectCommandsCount"), indirectDrawCountBufferDef, batchBuffersAllocInfo);
	graphBuilder.FillBuffer(RG_DEBUG_NAME("SetSMDepthPrepassIndirectCommandsDefaultCount"), indirectDrawCountBuffer, 0, indirectDrawsCountSize, 0);

	// Create Descriptor Sets
	
	const lib::SharedRef<SMDepthPrepassCullInstancesDS> cullInstancesDS = rdr::ResourcesManager::CreateDescriptorSetState<SMDepthPrepassCullInstancesDS>(RENDERER_RESOURCE_NAME("SMDepthPrepassCullInstancesDS"));
	cullInstancesDS->u_drawCommands				= drawCommandsBuffer;
	cullInstancesDS->u_drawsCount				= indirectDrawCountBuffer;
	cullInstancesDS->u_validBatchElementsNum	= rdr::BufferView(lib::Ref(batchElements.batchElementsDispatchParamsBuffer), 0, sizeof(Uint32));

	const lib::SharedRef<SMDepthOnlyDrawInstancesDS> drawInstancesDS = rdr::ResourcesManager::CreateDescriptorSetState<SMDepthOnlyDrawInstancesDS>(RENDERER_RESOURCE_NAME("SMDepthPrepassDrawInstancesDS"));
	drawInstancesDS->u_drawCommands = drawCommandsBuffer;

	batch.cullInstancesDS = cullInstancesDS;
	batch.drawInstancesDS = drawInstancesDS;

	batch.drawCommandsBuffer = drawCommandsBuffer;
	batch.indirectDrawCountBuffer = indirectDrawCountBuffer;

	return batch;
}

} // spt::rsc
