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

namespace spt::rsc
{

StaticMeshForwardOpaqueRenderer::StaticMeshForwardOpaqueRenderer()
{
	{
		sc::ShaderCompilationSettings compilationSettings;
		compilationSettings.AddShaderToCompile(sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "CullSubmeshesCS"));
		const rdr::ShaderID cullSubmeshesShader = rdr::ResourcesManager::CreateShader("Sculptor/StaticMeshes/StaticMesh_CullSubmeshes.hlsl", compilationSettings);
		m_cullSubmeshesPipeline = rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("StaticMesh_CullSubmeshesPipeline"), cullSubmeshesShader);
	}
	
	{
		sc::ShaderCompilationSettings compilationSettings;
		compilationSettings.AddShaderToCompile(sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "CullMeshletsCS"));
		const rdr::ShaderID cullMeshletsShader = rdr::ResourcesManager::CreateShader("Sculptor/StaticMeshes/StaticMesh_CullMeshlets.hlsl", compilationSettings);
		m_cullMeshletsPipeline = rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("StaticMesh_CullMeshletsPipeline"), cullMeshletsShader);
	}

	{
		sc::ShaderCompilationSettings compilationSettings;
		compilationSettings.AddShaderToCompile(sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "CullTrianglesCS"));
		const rdr::ShaderID cullTrianglesShader = rdr::ResourcesManager::CreateShader("Sculptor/StaticMeshes/StaticMesh_CullTriangles.hlsl", compilationSettings);
		m_cullTrianglesPipeline = rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("StaticMesh_CullTrianglesPipeline"), cullTrianglesShader);
	}

	{
		const RenderTargetFormatsDef forwardOpaqueRTFormats = ForwardOpaqueRenderStage::GetRenderTargetFormats();

		sc::ShaderCompilationSettings compilationSettings;
		compilationSettings.AddShaderToCompile(sc::ShaderStageCompilationDef(rhi::EShaderStage::Vertex, "StaticMeshVS"));
		compilationSettings.AddShaderToCompile(sc::ShaderStageCompilationDef(rhi::EShaderStage::Fragment, "StaticMeshFS"));
		const rdr::ShaderID forwardOpaqueShader = rdr::ResourcesManager::CreateShader("Sculptor/StaticMeshes/StaticMesh_ForwardOpaqueShading.hlsl", compilationSettings);

		rhi::GraphicsPipelineDefinition pipelineDef;
		pipelineDef.primitiveTopology = rhi::EPrimitiveTopology::TriangleList;
		pipelineDef.renderTargetsDefinition.depthRTDefinition.format = forwardOpaqueRTFormats.depthRTFormat;
		pipelineDef.renderTargetsDefinition.depthRTDefinition.depthCompareOp = spt::rhi::EDepthCompareOperation::GreaterOrEqual;
		for (const rhi::EFragmentFormat format : forwardOpaqueRTFormats.colorRTFormats)
		{
			pipelineDef.renderTargetsDefinition.colorRTsDefinition.emplace_back(rhi::ColorRenderTargetDefinition(format));
		}

		m_forwadOpaqueShadingPipeline = rdr::ResourcesManager::CreateGfxPipeline(RENDERER_RESOURCE_NAME("StaticMesh_ForwardOpaqueShading"), pipelineDef, forwardOpaqueShader);
	}
}

Bool StaticMeshForwardOpaqueRenderer::BuildBatchesPerView(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec)
{
	SPT_PROFILER_FUNCTION()

	const StaticMeshPrimitivesSystem& staticMeshPrimsSystem = renderScene.GetPrimitivesSystemChecked<StaticMeshPrimitivesSystem>();

	const StaticMeshBatchDefinition batchDef = staticMeshPrimsSystem.BuildBatchForView(viewSpec.GetRenderView());

	// We don't need array here right now, but it's made for future use, as we'll for sure want to have more than one batch
	lib::DynamicArray<SMForwardOpaqueBatch> batches;

	if (batchDef.IsValid())
	{
		const SMForwardOpaqueBatch batchRenderingData = CreateBatch(graphBuilder, renderScene, batchDef);
		batches.emplace_back(batchRenderingData);
	}

	if (!batches.empty())
	{
		// if view spec supports depth prepass, we need to cache visible instances to render them next frame during prepass
		if (viewSpec.SupportsStage(ERenderStage::DepthPrepass))
		{
			// store data in entity data
			// This way it will be available next frames
			const RenderView& renderView = viewSpec.GetRenderView();
			RenderSceneEntityHandle viewEntityHandle = renderView.GetViewEntity();

			StaticMeshesVisibleLastFrame& visibleInstances = viewEntityHandle.emplace_or_replace<StaticMeshesVisibleLastFrame>();

			std::transform(std::cbegin(batches), std::cend(batches),
						   std::back_inserter(visibleInstances.visibleInstances),
						   [](const SMForwardOpaqueBatch& batchData)
						   {
							   StaticMeshesVisibleLastFrame::BatchElementsInfo instances;
							   instances.visibleBatchElementsBuffer			= batchData.visibleBatchElementsBufferInstance;
							   instances.batchElementsDispatchParamsBuffer	= batchData.dispatchVisibleBatchElemsParamsBufferInstance;
							   instances.batchedSubmeshesNum				= batchData.batchedSubmeshesNum;
							   return instances;
						   });
		}

		SMOpaqueForwardBatches& viewBatches = viewSpec.GetData().Create<SMOpaqueForwardBatches>();
		viewBatches.batches = std::move(batches);

		return true;
	}

	return false;
}

void StaticMeshForwardOpaqueRenderer::CullPerView(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& context)
{
	SPT_PROFILER_FUNCTION();

	const DepthPrepassData& prepassData = viewSpec.GetData().Get<DepthPrepassData>();

	const SMOpaqueForwardBatches& forwardOpaqueBatches = viewSpec.GetData().Get<SMOpaqueForwardBatches>();

	const SMRenderingViewData& staticMeshRenderingViewData = viewSpec.GetData().Get<SMRenderingViewData>();

	const rg::BindDescriptorSetsScope staticMeshCullingDSScope(graphBuilder,
															   rg::BindDescriptorSets(lib::Ref(StaticMeshUnifiedData::Get().GetUnifiedDataDS()),
																					  lib::Ref(staticMeshRenderingViewData.viewDS),
																					  lib::Ref(prepassData.depthCullingDS)));

	for (const SMForwardOpaqueBatch& batch : forwardOpaqueBatches.batches)
	{
		const rg::BindDescriptorSetsScope staticMeshBatchDSScope(graphBuilder, rg::BindDescriptorSets(lib::Ref(batch.batchDS)));

		const Uint32 dispatchGroupsNum = math::Utils::RoundUp<Uint32>(batch.batchedSubmeshesNum, 64) / 64;

		graphBuilder.Dispatch(RG_DEBUG_NAME("SM Cull Submeshes"),
							  m_cullSubmeshesPipeline,
							  math::Vector3u(dispatchGroupsNum, 1, 1),
							  rg::BindDescriptorSets(lib::Ref(batch.cullSubmeshesDS)));
	}

	for (const SMForwardOpaqueBatch& batch : forwardOpaqueBatches.batches)
	{
		const rg::BindDescriptorSetsScope staticMeshBatchDSScope(graphBuilder, rg::BindDescriptorSets(lib::Ref(batch.batchDS)));

		graphBuilder.DispatchIndirect(RG_DEBUG_NAME("SM Cull Meshlets"),
									  m_cullMeshletsPipeline,
									  batch.visibleBatchElementsDispatchParamsBuffer,
									  0,
									  rg::BindDescriptorSets(lib::Ref(batch.cullMeshletsDS)));
	}

	for (const SMForwardOpaqueBatch& batch : forwardOpaqueBatches.batches)
	{
		const rg::BindDescriptorSetsScope staticMeshBatchDSScope(graphBuilder, rg::BindDescriptorSets(lib::Ref(batch.batchDS)));

		graphBuilder.DispatchIndirect(RG_DEBUG_NAME("SM Cull Triangles"),
									  m_cullTrianglesPipeline,
									  batch.meshletsWorkloadsDispatchArgsBuffer,
									  0,
									  rg::BindDescriptorSets(lib::Ref(batch.cullTrianglesDS),
															 lib::Ref(GeometryManager::Get().GetGeometryDSState())));
	}
}

void StaticMeshForwardOpaqueRenderer::RenderPerView(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& context)
{
	SPT_PROFILER_FUNCTION();

	const SMOpaqueForwardBatches& forwardOpaqueBatches = viewSpec.GetData().Get<SMOpaqueForwardBatches>();

	const SMRenderingViewData& staticMeshRenderingViewData = viewSpec.GetData().Get<SMRenderingViewData>();

	const rg::BindDescriptorSetsScope staticMeshRenderingDSScope(graphBuilder,
																 rg::BindDescriptorSets(lib::Ref(StaticMeshUnifiedData::Get().GetUnifiedDataDS()),
																						lib::Ref(GeometryManager::Get().GetGeometryDSState()),
																						lib::Ref(staticMeshRenderingViewData.viewDS)));

	for (const SMForwardOpaqueBatch& batch : forwardOpaqueBatches.batches)
	{
		const rg::BindDescriptorSetsScope staticMeshBatchDSScope(graphBuilder, rg::BindDescriptorSets(lib::Ref(batch.batchDS)));

		SMIndirectTrianglesBatchDrawParams drawParams;
		drawParams.batchDrawCommandsBuffer = batch.drawTrianglesBatchArgsBuffer;

		const lib::SharedRef<GeometryDS> unifiedGeometryDS = lib::Ref(GeometryManager::Get().GetGeometryDSState());

		graphBuilder.AddSubpass(RG_DEBUG_NAME("Render Static Meshes Batch"),
								rg::BindDescriptorSets(lib::Ref(batch.indirectRenderTrianglesDS),
													   MaterialsUnifiedData::Get().GetMaterialsDS()),
								std::tie(drawParams),
								[drawParams, this](const lib::SharedRef<rdr::RenderContext>& renderContext, rdr::CommandRecorder& recorder)
								{
									recorder.BindGraphicsPipeline(m_forwadOpaqueShadingPipeline);

									const rdr::BufferView& drawsBufferView = drawParams.batchDrawCommandsBuffer->GetResource();
									recorder.DrawIndirect(drawsBufferView, 0, sizeof(SMIndirectDrawCallData), 1);
								});
	}
}

SMForwardOpaqueBatch StaticMeshForwardOpaqueRenderer::CreateBatch(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, const StaticMeshBatchDefinition& batchDef) const
{
	SPT_PROFILER_FUNCTION();
	
	SMForwardOpaqueBatch batch;

	// Create GPU batch data

	SMGPUBatchData gpuBatchData;
	gpuBatchData.elementsNum = static_cast<Uint32>(batchDef.batchElements.size());

	// Create Batch elements buffer

	const Uint64 batchDataSize = sizeof(StaticMeshBatchElement) * batchDef.batchElements.size();
	const rhi::BufferDefinition batchBufferDef(batchDataSize, rhi::EBufferUsage::Storage);
	const rhi::RHIAllocationInfo batchBufferAllocation(rhi::EMemoryUsage::CPUToGPU);
	const lib::SharedRef<rdr::Buffer> batchBuffer = rdr::ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME("StaticMeshesBatch"), batchBufferDef, batchBufferAllocation);

	gfx::UploadDataToBuffer(batchBuffer, 0, reinterpret_cast<const Byte*>(batchDef.batchElements.data()), batchDataSize);

	batch.batchDS = rdr::ResourcesManager::CreateDescriptorSetState<StaticMeshBatchDS>(RENDERER_RESOURCE_NAME("StaticMeshesBatchDS"));
	batch.batchDS->u_batchElements = batchBuffer->CreateFullView();
	batch.batchDS->u_batchData = gpuBatchData;

	batch.batchedSubmeshesNum = static_cast<Uint32>(batchDef.batchElements.size());

	const rhi::EBufferUsage indirectBuffersUsageFlags = lib::Flags(rhi::EBufferUsage::Storage, rhi::EBufferUsage::Indirect, rhi::EBufferUsage::TransferDst);
	const rhi::RHIAllocationInfo batchBuffersAllocation(rhi::EMemoryUsage::GPUOnly);
	
	// Create workloads buffers

	// visible batch elements buffer is not created using render graph because it's lifetime must be longer than 1 frame
	// That's because we will use this buffer to render depth prepass next frame
	const rhi::BufferDefinition visibleInstancesBufferDef(sizeof(StaticMeshBatchElement) * batchDef.batchElements.size(), rhi::EBufferUsage::Storage);
	const lib::SharedRef<rdr::Buffer> visibleBatchElementsBufferInstance = rdr::ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME("SMVisibleBatchElements"), visibleInstancesBufferDef, batchBuffersAllocation);

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

	// This also lives longer than frame, so it's not created using render graph
	const lib::SharedRef<rdr::Buffer> dispatchVisibleBatchElemsParamsBufferInstance = rdr::ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME("DispatchVisibleBatchElementsParams"), indirectDispatchesParamsBufferDef, batchBuffersAllocation);
	const rg::RGBufferViewHandle dispatchVisibleBatchElemsParamsBuffer = graphBuilder.AcquireExternalBufferView(dispatchVisibleBatchElemsParamsBufferInstance->CreateFullView());
	graphBuilder.FillBuffer(RG_DEBUG_NAME("SetDefaultDrawTrianglesParams"), dispatchVisibleBatchElemsParamsBuffer, 0, indirectDispatchParamsSize, 0);

	batch.visibleBatchElementsBufferInstance				= visibleBatchElementsBufferInstance;
	batch.dispatchVisibleBatchElemsParamsBufferInstance		= dispatchVisibleBatchElemsParamsBufferInstance;

	batch.visibleBatchElementsDispatchParamsBuffer	= dispatchVisibleBatchElemsParamsBuffer;
	batch.meshletsWorkloadsDispatchArgsBuffer		= dispatchMeshletsParamsBuffer;
	batch.drawTrianglesBatchArgsBuffer				= drawTrianglesBatchParamsBuffer;

	const rg::RGBufferViewHandle visibleBatchElements = graphBuilder.AcquireExternalBufferView(visibleBatchElementsBufferInstance->CreateFullView());

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

} // spt::rsc