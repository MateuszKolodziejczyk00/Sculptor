#include "StaticMeshDepthPrepassRenderer.h"
#include "Common/ShaderCompilationInput.h"
#include "ResourcesManager.h"
#include "View/ViewRenderingSpec.h"
#include "RenderGraphBuilder.h"
#include "GeometryManager.h"
#include "StaticMeshGeometry.h"
#include "SceneRenderer/RenderStages/DepthPrepassRenderStage.h"
#include "MaterialsUnifiedData.h"
#include "MaterialsSubsystem.h"

namespace spt::rsc
{

StaticMeshDepthPrepassRenderer::StaticMeshDepthPrepassRenderer()
{
	{
		const rdr::ShaderID buildDrawCommandsShader = rdr::ResourcesManager::CreateShader("Sculptor/StaticMeshes/StaticMesh_BuildDepthPrepassDrawCommands.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "BuildDrawCommandsCS"));
		m_buildDrawCommandsPipeline = rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("StaticMesh_BuildDepthPrepassDrawCommands"), buildDrawCommandsShader);
	}
}

Bool StaticMeshDepthPrepassRenderer::BuildBatchesPerView(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const lib::DynamicArray<StaticMeshBatchDefinition>& batchDefinitions)
{
	SPT_PROFILER_FUNCTION();
	
	lib::DynamicArray<SMDepthPrepassBatch> batches;
	batches.reserve(batchDefinitions.size());

	std::transform(std::cbegin(batchDefinitions), std::cend(batchDefinitions),
				   std::back_inserter(batches),
				   [&](const StaticMeshBatchDefinition& batchDefinition)
				   {
					   return CreateBatch(graphBuilder, renderScene, batchDefinition);
				   });

	if (!batches.empty())
	{
		SMDepthPrepassBatches& viewBatches = viewSpec.GetData().Create<SMDepthPrepassBatches>();
		viewBatches.batches = std::move(batches);

		return true;
	}

	return false;
}

void StaticMeshDepthPrepassRenderer::CullPerView(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec)
{
	SPT_PROFILER_FUNCTION();

	const RenderView& renderView = viewSpec.GetRenderView();

	const SMDepthPrepassBatches& depthPrepassBatches = viewSpec.GetData().Get<SMDepthPrepassBatches>();

	const rg::BindDescriptorSetsScope staticMeshCullingDSScope(graphBuilder,
															   rg::BindDescriptorSets(StaticMeshUnifiedData::Get().GetUnifiedDataDS(),
																					  renderView.GetRenderViewDS()));

	for (const SMDepthPrepassBatch& batch : depthPrepassBatches.batches)
	{
		const rg::BindDescriptorSetsScope staticMeshBatchDSScope(graphBuilder, rg::BindDescriptorSets(batch.batchDS));

		const Uint32 dispatchGroupsNum = math::Utils::RoundUp<Uint32>(batch.batchedSubmeshesNum, 64) / 64;

		graphBuilder.Dispatch(RG_DEBUG_NAME("SM Cull And Build Draw Commands"),
							  m_buildDrawCommandsPipeline,
							  math::Vector3u(dispatchGroupsNum, 1, 1),
							  rg::BindDescriptorSets(batch.cullInstancesDS));
	}
}

void StaticMeshDepthPrepassRenderer::RenderPerView(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& context)
{
	SPT_PROFILER_FUNCTION();

	const RenderView& renderView = viewSpec.GetRenderView();

	const SMDepthPrepassBatches& depthPrepassBatches = viewSpec.GetData().Get<SMDepthPrepassBatches>();

	const rg::BindDescriptorSetsScope staticMeshRenderingDSScope(graphBuilder,
																 rg::BindDescriptorSets(StaticMeshUnifiedData::Get().GetUnifiedDataDS(),
																						GeometryManager::Get().GetGeometryDSState(),
																						renderView.GetRenderViewDS(),
																						mat::MaterialsUnifiedData::Get().GetMaterialsDS()));

	for (const SMDepthPrepassBatch& batch : depthPrepassBatches.batches)
	{
		SMIndirectDepthPrepassCommandsParameters drawParams;
		drawParams.batchDrawCommandsBuffer = batch.drawCommandsBuffer;
		drawParams.batchDrawCommandsCountBuffer = batch.indirectDrawCountBuffer;

		const Uint32 maxDrawCallsNum = batch.batchedSubmeshesNum;

		graphBuilder.AddSubpass(RG_DEBUG_NAME("Render Static Meshes Batch"),
								rg::BindDescriptorSets(batch.drawInstancesDS, batch.batchDS),
								std::tie(drawParams),
								[pipelineID = GetPipelineStateForBatch(batch), maxDrawCallsNum, drawParams, this](const lib::SharedRef<rdr::RenderContext>& renderContext, rdr::CommandRecorder& recorder)
								{
									recorder.BindGraphicsPipeline(pipelineID);

									const rdr::BufferView& drawsBufferView = drawParams.batchDrawCommandsBuffer->GetResource();
									const rdr::BufferView& drawCountBufferView = drawParams.batchDrawCommandsCountBuffer->GetResource();
									recorder.DrawIndirectCount(drawsBufferView, 0, sizeof(SMDepthOnlyDrawCallData), drawCountBufferView, 0, maxDrawCallsNum);
								});
	}
}

SMDepthPrepassBatch StaticMeshDepthPrepassRenderer::CreateBatch(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, const StaticMeshBatchDefinition& batchDef) const
{
	SPT_PROFILER_FUNCTION();

	SMDepthPrepassBatch batch;

	// Create batch info

	batch.batchDS				= batchDef.batchDS;
	batch.batchedSubmeshesNum	= batchDef.batchElementsNum;
	batch.materialShadersHash	= batchDef.materialShadersHash;

	// Create buffers

	const rhi::RHIAllocationInfo batchBuffersAllocInfo(rhi::EMemoryUsage::GPUOnly);

	const rhi::BufferDefinition commandsBufferDef(sizeof(SMDepthOnlyDrawCallData) * batch.batchedSubmeshesNum, lib::Flags(rhi::EBufferUsage::Storage, rhi::EBufferUsage::Indirect));
	const rg::RGBufferViewHandle drawCommandsBuffer = graphBuilder.CreateBufferView(RG_DEBUG_NAME("SMDepthPrepassIndirectCommands"), commandsBufferDef, batchBuffersAllocInfo);

	const Uint64 indirectDrawsCountSize = sizeof(Uint32);
	const rhi::BufferDefinition indirectDrawCountBufferDef(indirectDrawsCountSize, lib::Flags(rhi::EBufferUsage::Storage, rhi::EBufferUsage::Indirect, rhi::EBufferUsage::TransferDst));
	const rg::RGBufferViewHandle indirectDrawCountBuffer = graphBuilder.CreateBufferView(RG_DEBUG_NAME("SMDepthPrepassIndirectCommandsCount"), indirectDrawCountBufferDef, batchBuffersAllocInfo);
	graphBuilder.FillBuffer(RG_DEBUG_NAME("SetSMDepthPrepassIndirectCommandsDefaultCount"), indirectDrawCountBuffer, 0, indirectDrawsCountSize, 0);

	// Create Descriptor Sets
	
	const lib::SharedRef<SMDepthPrepassCullInstancesDS> cullInstancesDS = rdr::ResourcesManager::CreateDescriptorSetState<SMDepthPrepassCullInstancesDS>(RENDERER_RESOURCE_NAME("SMDepthPrepassCullInstancesDS"));
	cullInstancesDS->u_drawCommands				= drawCommandsBuffer;
	cullInstancesDS->u_drawsCount				= indirectDrawCountBuffer;

	const lib::SharedRef<SMDepthOnlyDrawInstancesDS> drawInstancesDS = rdr::ResourcesManager::CreateDescriptorSetState<SMDepthOnlyDrawInstancesDS>(RENDERER_RESOURCE_NAME("SMDepthPrepassDrawInstancesDS"));
	drawInstancesDS->u_drawCommands = drawCommandsBuffer;

	batch.cullInstancesDS = cullInstancesDS;
	batch.drawInstancesDS = drawInstancesDS;

	batch.drawCommandsBuffer = drawCommandsBuffer;
	batch.indirectDrawCountBuffer = indirectDrawCountBuffer;

	return batch;
}

rdr::PipelineStateID StaticMeshDepthPrepassRenderer::GetPipelineStateForBatch(const SMDepthPrepassBatch& batch) const
{
	SPT_PROFILER_FUNCTION();

	const mat::MaterialGraphicsShaders materialShaders = mat::MaterialsSubsystem::Get().GetMaterialShaders<mat::MaterialGraphicsShaders>("SMDepthOnly",
																																		 batch.materialShadersHash);
	rdr::GraphicsPipelineShaders shaders;
	shaders.vertexShader	= materialShaders.vertexShader;
	shaders.fragmentShader	= materialShaders.fragmentShader;

	rhi::GraphicsPipelineDefinition depthPrepassPipelineDef;
	depthPrepassPipelineDef.primitiveTopology = rhi::EPrimitiveTopology::TriangleList;
	depthPrepassPipelineDef.renderTargetsDefinition.depthRTDefinition.format = DepthPrepassRenderStage::GetDepthFormat();
	return rdr::ResourcesManager::CreateGfxPipeline(RENDERER_RESOURCE_NAME("StaticMesh_DepthPrepassPipeline"), shaders, depthPrepassPipelineDef);
}

} // spt::rsc
