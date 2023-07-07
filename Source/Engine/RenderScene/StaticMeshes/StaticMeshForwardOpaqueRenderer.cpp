#include "StaticMeshForwardOpaqueRenderer.h"
#include "RenderGraphBuilder.h"
#include "GeometryManager.h"
#include "StaticMeshGeometry.h"
#include "Materials/MaterialsUnifiedData.h"
#include "Common/ShaderCompilationInput.h"
#include "ResourcesManager.h"
#include "Transfers/UploadUtils.h"
#include "SceneRenderer/RenderStages/ForwardOpaqueRenderStage.h"
#include "View/ViewRenderingSpec.h"
#include "RenderScene.h"
#include "SceneRenderer/SceneRendererTypes.h"
#include "Shadows/ShadowMapsManagerSubsystem.h"
#include "DDGI/DDGISceneSubsystem.h"

namespace spt::rsc
{

StaticMeshForwardOpaqueRenderer::StaticMeshForwardOpaqueRenderer()
{
	{
		const rdr::ShaderID cullSubmeshesShader = rdr::ResourcesManager::CreateShader("Sculptor/StaticMeshes/StaticMesh_CullSubmeshes.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "CullSubmeshesCS"));
		m_cullSubmeshesPipeline = rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("StaticMesh_CullSubmeshesPipeline"), cullSubmeshesShader);
	}
	
	{
		const rdr::ShaderID cullMeshletsShader = rdr::ResourcesManager::CreateShader("Sculptor/StaticMeshes/StaticMesh_CullMeshlets.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "CullMeshletsCS"));
		m_cullMeshletsPipeline = rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("StaticMesh_CullMeshletsPipeline"), cullMeshletsShader);
	}

	{
		const rdr::ShaderID cullTrianglesShader = rdr::ResourcesManager::CreateShader("Sculptor/StaticMeshes/StaticMesh_CullTriangles.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "CullTrianglesCS"));
		m_cullTrianglesPipeline = rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("StaticMesh_CullTrianglesPipeline"), cullTrianglesShader);
	}
}

Bool StaticMeshForwardOpaqueRenderer::BuildBatchesPerView(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const StaticMeshBatchDefinition& batchDefinition, const lib::SharedRef<StaticMeshBatchDS>& batchDS)
{
	SPT_PROFILER_FUNCTION()

	// We don't need array here right now, but it's made for future use, as we'll for sure want to have more than one batch
	lib::DynamicArray<SMForwardOpaqueBatch> batches;

	if (batchDefinition.IsValid())
	{
		const SMForwardOpaqueBatch batchRenderingData = CreateBatch(graphBuilder, renderScene, batchDefinition, batchDS);
		batches.emplace_back(batchRenderingData);
	}

	if (!batches.empty())
	{
		SMOpaqueForwardBatches& viewBatches = viewSpec.GetData().Create<SMOpaqueForwardBatches>();
		viewBatches.batches = std::move(batches);

		return true;
	}

	return false;
}

void StaticMeshForwardOpaqueRenderer::CullPerView(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& context)
{
	SPT_PROFILER_FUNCTION();

	const RenderView& renderView = viewSpec.GetRenderView();

	const DepthPrepassData& prepassData = viewSpec.GetData().Get<DepthPrepassData>();

	const SMOpaqueForwardBatches& forwardOpaqueBatches = viewSpec.GetData().Get<SMOpaqueForwardBatches>();

	const rg::BindDescriptorSetsScope staticMeshCullingDSScope(graphBuilder,
															   rg::BindDescriptorSets(StaticMeshUnifiedData::Get().GetUnifiedDataDS(),
																					  renderView.GetRenderViewDS(),
																					  prepassData.depthCullingDS));

	for (const SMForwardOpaqueBatch& batch : forwardOpaqueBatches.batches)
	{
		const rg::BindDescriptorSetsScope staticMeshBatchDSScope(graphBuilder, rg::BindDescriptorSets(batch.batchDS));

		const Uint32 dispatchGroupsNum = math::Utils::RoundUp<Uint32>(batch.batchedSubmeshesNum, 64) / 64;

		graphBuilder.Dispatch(RG_DEBUG_NAME("SM Cull Submeshes"),
							  m_cullSubmeshesPipeline,
							  math::Vector3u(dispatchGroupsNum, 1, 1),
							  rg::BindDescriptorSets(batch.cullSubmeshesDS));
	}

	for (const SMForwardOpaqueBatch& batch : forwardOpaqueBatches.batches)
	{
		const rg::BindDescriptorSetsScope staticMeshBatchDSScope(graphBuilder, rg::BindDescriptorSets(batch.batchDS));

		graphBuilder.DispatchIndirect(RG_DEBUG_NAME("SM Cull Meshlets"),
									  m_cullMeshletsPipeline,
									  batch.visibleBatchElementsDispatchParamsBuffer,
									  0,
									  rg::BindDescriptorSets(batch.cullMeshletsDS));
	}

	for (const SMForwardOpaqueBatch& batch : forwardOpaqueBatches.batches)
	{
		const rg::BindDescriptorSetsScope staticMeshBatchDSScope(graphBuilder, rg::BindDescriptorSets(batch.batchDS));

		graphBuilder.DispatchIndirect(RG_DEBUG_NAME("SM Cull Triangles"),
									  m_cullTrianglesPipeline,
									  batch.meshletsWorkloadsDispatchArgsBuffer,
									  0,
									  rg::BindDescriptorSets(batch.cullTrianglesDS,
															 GeometryManager::Get().GetGeometryDSState()));
	}
}

void StaticMeshForwardOpaqueRenderer::RenderPerView(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& context)
{
	SPT_PROFILER_FUNCTION();

	const RenderView& renderView = viewSpec.GetRenderView();

	const SMOpaqueForwardBatches& forwardOpaqueBatches = viewSpec.GetData().Get<SMOpaqueForwardBatches>();

	const rg::BindDescriptorSetsScope staticMeshRenderingDSScope(graphBuilder,
																 rg::BindDescriptorSets(StaticMeshUnifiedData::Get().GetUnifiedDataDS(),
																						GeometryManager::Get().GetGeometryDSState(),
																						renderView.GetRenderViewDS()));

	const lib::SharedPtr<DDGISceneSubsystem> ddgiSceneSubsystem = renderScene.GetSceneSubsystem<DDGISceneSubsystem>();
	const lib::SharedPtr<DDGIDS> ddgiDS = ddgiSceneSubsystem ? ddgiSceneSubsystem->GetDDGIDS() : nullptr;

	for (const SMForwardOpaqueBatch& batch : forwardOpaqueBatches.batches)
	{
		const rg::BindDescriptorSetsScope staticMeshBatchDSScope(graphBuilder, rg::BindDescriptorSets(batch.batchDS));

		SMIndirectTrianglesBatchDrawParams drawParams;
		drawParams.batchDrawCommandsBuffer = batch.drawTrianglesBatchArgsBuffer;

		graphBuilder.AddSubpass(RG_DEBUG_NAME("Render Static Meshes Batch"),
								rg::BindDescriptorSets(batch.indirectRenderTrianglesDS,
													   MaterialsUnifiedData::Get().GetMaterialsDS(),
													   ddgiDS),
								std::tie(drawParams),
								[drawParams, this, pipeline = GetShadingPipeline(renderScene, viewSpec)](const lib::SharedRef<rdr::RenderContext>& renderContext, rdr::CommandRecorder& recorder)
								{
									recorder.BindGraphicsPipeline(pipeline);

									const rdr::BufferView& drawsBufferView = drawParams.batchDrawCommandsBuffer->GetResource();
									recorder.DrawIndirect(drawsBufferView, 0, sizeof(SMIndirectDrawCallData), 1);
								});
	}
}

SMForwardOpaqueBatch StaticMeshForwardOpaqueRenderer::CreateBatch(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, const StaticMeshBatchDefinition& batchDef, const lib::SharedRef<StaticMeshBatchDS>& batchDS) const
{
	SPT_PROFILER_FUNCTION();
	
	SMForwardOpaqueBatch batch;

	batch.batchDS = batchDS;

	batch.batchedSubmeshesNum = static_cast<Uint32>(batchDef.batchElements.size());

	const rhi::EBufferUsage indirectBuffersUsageFlags = lib::Flags(rhi::EBufferUsage::Storage, rhi::EBufferUsage::Indirect, rhi::EBufferUsage::TransferDst);
	const rhi::RHIAllocationInfo batchBuffersAllocation(rhi::EMemoryUsage::GPUOnly);
	
	// Create workloads buffers

	const rhi::BufferDefinition visibleInstancesBufferDef(sizeof(StaticMeshBatchElement) * batchDef.batchElements.size(), rhi::EBufferUsage::Storage);
	const rg::RGBufferViewHandle visibleBatchElements = graphBuilder.CreateBufferView(RG_DEBUG_NAME("SMVisibleBatchElements"), visibleInstancesBufferDef, batchBuffersAllocation);

	const rhi::BufferDefinition meshletWorkloadsBufferDef(sizeof(SMGPUWorkloadID) * batchDef.maxMeshletsNum, rhi::EBufferUsage::Storage);
	const rg::RGBufferViewHandle meshletWorkloadsBuffer = graphBuilder.CreateBufferView(RG_DEBUG_NAME("SMMeshletWorkloads"), meshletWorkloadsBufferDef, batchBuffersAllocation);

	const rhi::BufferDefinition triangleWorkloadsBufferDef(sizeof(SMGPUWorkloadID) * batchDef.maxTrianglesNum, rhi::EBufferUsage::Storage);
	const rg::RGBufferViewHandle triangleWorkloadsBuffer = graphBuilder.CreateBufferView(RG_DEBUG_NAME("SMTriangleWorkloads"), triangleWorkloadsBufferDef, batchBuffersAllocation);

	// Create indirect parameters buffers

	const Uint64 indirectDispatchParamsSize = 3 * sizeof(Uint32);
	const Uint64 indirectDrawParamsSize = sizeof(SMIndirectDrawCallData);

	const rhi::BufferDefinition indirectDispatchesParamsBufferDef(indirectDispatchParamsSize, indirectBuffersUsageFlags);
	const rhi::BufferDefinition indirectDrawsParamsBufferDef(indirectDrawParamsSize, indirectBuffersUsageFlags);

	const rg::RGBufferViewHandle dispatchMeshletsParamsBuffer = graphBuilder.CreateBufferView(RG_DEBUG_NAME("DispatchMeshletsParams"), indirectDispatchesParamsBufferDef, batchBuffersAllocation);
	graphBuilder.FillBuffer(RG_DEBUG_NAME("SetDefaultDispatchMeshletsParams"), dispatchMeshletsParamsBuffer, 0, indirectDispatchParamsSize, 0);

	const rg::RGBufferViewHandle drawTrianglesBatchParamsBuffer = graphBuilder.CreateBufferView(RG_DEBUG_NAME("DrawTrianglesBatchParams"), indirectDrawsParamsBufferDef, batchBuffersAllocation);
	graphBuilder.FillBuffer(RG_DEBUG_NAME("SetDefaultDrawTrianglesParams"), drawTrianglesBatchParamsBuffer, 0, indirectDrawParamsSize, 0);

	const rg::RGBufferViewHandle dispatchVisibleBatchElemsParamsBuffer = graphBuilder.CreateBufferView(RG_DEBUG_NAME("DispatchVisibleBatchElementsParams"), indirectDispatchesParamsBufferDef, batchBuffersAllocation);
	graphBuilder.FillBuffer(RG_DEBUG_NAME("SetDefaultDrawTrianglesParams"), dispatchVisibleBatchElemsParamsBuffer, 0, indirectDispatchParamsSize, 0);

	batch.visibleBatchElementsDispatchParamsBuffer	= dispatchVisibleBatchElemsParamsBuffer;
	batch.meshletsWorkloadsDispatchArgsBuffer		= dispatchMeshletsParamsBuffer;
	batch.drawTrianglesBatchArgsBuffer				= drawTrianglesBatchParamsBuffer;

	// Create descriptor set states

	const lib::SharedRef<SMCullSubmeshesDS> cullSubmeshesDS = rdr::ResourcesManager::CreateDescriptorSetState<SMCullSubmeshesDS>(RENDERER_RESOURCE_NAME("BatchCullSubmeshesDS"));
	cullSubmeshesDS->u_visibleBatchElements				= visibleBatchElements;
	cullSubmeshesDS->u_dispatchVisibleBatchElemsParams	= dispatchVisibleBatchElemsParamsBuffer;

	const lib::SharedRef<SMCullMeshletsDS> cullMeshletsDS = rdr::ResourcesManager::CreateDescriptorSetState<SMCullMeshletsDS>(RENDERER_RESOURCE_NAME("BatchCullMeshletsDS"));
	cullMeshletsDS->u_visibleBatchElements		= visibleBatchElements;
	cullMeshletsDS->u_meshletWorkloads			= meshletWorkloadsBuffer;
	cullMeshletsDS->u_dispatchMeshletsParams	= dispatchMeshletsParamsBuffer;

	const lib::SharedRef<SMCullTrianglesDS> cullTrianglesDS = rdr::ResourcesManager::CreateDescriptorSetState<SMCullTrianglesDS>(RENDERER_RESOURCE_NAME("BatchCullTrianglesDS"));
	cullTrianglesDS->u_visibleBatchElements		= visibleBatchElements;
	cullTrianglesDS->u_meshletWorkloads			= meshletWorkloadsBuffer;
	cullTrianglesDS->u_triangleWorkloads		= triangleWorkloadsBuffer;
	cullTrianglesDS->u_drawTrianglesParams		= drawTrianglesBatchParamsBuffer;

	const lib::SharedRef<SMIndirectRenderTrianglesDS> indirectRenderTrianglesDS = rdr::ResourcesManager::CreateDescriptorSetState<SMIndirectRenderTrianglesDS>(RENDERER_RESOURCE_NAME("BatchIndirectRenderTrianglesDS"));
	indirectRenderTrianglesDS->u_visibleBatchElements	= visibleBatchElements;
	indirectRenderTrianglesDS->u_triangleWorkloads		= triangleWorkloadsBuffer;

	batch.cullSubmeshesDS				= cullSubmeshesDS;
	batch.cullMeshletsDS				= cullMeshletsDS;
	batch.cullTrianglesDS				= cullTrianglesDS;
	batch.indirectRenderTrianglesDS		= indirectRenderTrianglesDS;

	return batch;
}

rdr::PipelineStateID StaticMeshForwardOpaqueRenderer::GetShadingPipeline(const RenderScene& renderScene, ViewRenderingSpec& viewSpec) const
{
	SPT_PROFILER_FUNCTION();

	const RenderTargetFormatsDef forwardOpaqueRTFormats = ForwardOpaqueRenderStage::GetRenderTargetFormats();

	const lib::SharedPtr<DDGISceneSubsystem> ddgiSubsystem = renderScene.GetSceneSubsystem<DDGISceneSubsystem>();
	
	sc::ShaderCompilationSettings compilationSettings;
	if (ddgiSubsystem && ddgiSubsystem->IsDDGIEnabled())
	{
		compilationSettings.AddMacroDefinition(sc::MacroDefinition("ENABLE_DDGI", "1"));
	}

	if (viewSpec.SupportsStage(ERenderStage::AmbientOcclusion))
	{
		compilationSettings.AddMacroDefinition(sc::MacroDefinition("ENABLE_AMBIENT_OCCLUSION", "1"));
	}

	rdr::GraphicsPipelineShaders shaders;
	shaders.vertexShader = rdr::ResourcesManager::CreateShader("Sculptor/StaticMeshes/StaticMesh_ForwardOpaqueShading.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Vertex, "StaticMeshVS"), compilationSettings);
	shaders.fragmentShader = rdr::ResourcesManager::CreateShader("Sculptor/StaticMeshes/StaticMesh_ForwardOpaqueShading.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Fragment, "StaticMeshFS"), compilationSettings);

	rhi::GraphicsPipelineDefinition pipelineDef;
	pipelineDef.primitiveTopology = rhi::EPrimitiveTopology::TriangleList;
	pipelineDef.renderTargetsDefinition.depthRTDefinition.format = forwardOpaqueRTFormats.depthRTFormat;
	pipelineDef.renderTargetsDefinition.depthRTDefinition.depthCompareOp = spt::rhi::ECompareOp::GreaterOrEqual;
	for (const rhi::EFragmentFormat format : forwardOpaqueRTFormats.colorRTFormats)
	{
		pipelineDef.renderTargetsDefinition.colorRTsDefinition.emplace_back(rhi::ColorRenderTargetDefinition(format));
	}

	return rdr::ResourcesManager::CreateGfxPipeline(RENDERER_RESOURCE_NAME("StaticMesh_ForwardOpaqueShading"), shaders, pipelineDef);
}

} // spt::rsc
