#include "StaticMeshesRenderSystem.h"
#include "StaticMeshPrimitivesSystem.h"
#include "ShaderStructs/ShaderStructsMacros.h"
#include "RenderGraphBuilder.h"
#include "View/ViewRenderingSpec.h"
#include "View/RenderView.h"
#include "RenderScene.h"
#include "DescriptorSetBindings/RWBufferBinding.h"
#include "RGDescriptorSetState.h"
#include "DescriptorSetBindings/ConstantBufferBinding.h"
#include "Common/ShaderCompilationInput.h"
#include "GeometryManager.h"
#include "StaticMeshGeometry.h"
#include "BufferUtilities.h"
#include "SceneRenderer/RenderStages/ForwardOpaqueRenderStage.h"

namespace spt::rsc
{

BEGIN_SHADER_STRUCT(, StaticMeshIndirectDrawCallData)
	SHADER_STRUCT_FIELD(Uint32, vertexCount)
	SHADER_STRUCT_FIELD(Uint32, instanceCount)
	SHADER_STRUCT_FIELD(Uint32, firstVertex)
	SHADER_STRUCT_FIELD(Uint32, firstInstance)
END_SHADER_STRUCT();


BEGIN_SHADER_STRUCT(, GPUWorkloadID)
	SHADER_STRUCT_FIELD(Uint32, data1)
	SHADER_STRUCT_FIELD(Uint32, data2)
END_SHADER_STRUCT();


BEGIN_SHADER_STRUCT(, SMGPUBatchData)
	SHADER_STRUCT_FIELD(Uint32, elementsNum)
END_SHADER_STRUCT();


DS_BEGIN(SMProcessBatchForViewDS, rg::RGDescriptorSetState<SMProcessBatchForViewDS>)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<SceneViewData>),			u_sceneView)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<SceneViewCullingData>),	u_cullingData)
DS_END();


DS_BEGIN(StaticMeshBatchDS, rg::RGDescriptorSetState<StaticMeshBatchDS>)
	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<StaticMeshBatchElement>),	u_batchElements)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<SMGPUBatchData>),			u_batchData)
DS_END();


DS_BEGIN(SMCullInstancesDS, rg::RGDescriptorSetState<SMCullInstancesDS>)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<Uint16>),			u_visibleInstancesIndices)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<Uint32>),			u_dispatchInstancesParams)
DS_END();


DS_BEGIN(SMCullSubmeshesDS, rg::RGDescriptorSetState<SMCullSubmeshesDS>)
	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<Uint16>),				u_visibleInstancesIndices)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<GPUWorkloadID>),		u_submeshWorkloads)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<Uint32>),			u_dispatchSubmeshesParams)
DS_END();


DS_BEGIN(SMCullMeshletsDS, rg::RGDescriptorSetState<SMCullMeshletsDS>)
	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<GPUWorkloadID>),		u_submeshWorkloads)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<GPUWorkloadID>),		u_meshletWorkloads)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<Uint32>),			u_dispatchMeshletsParams)
DS_END();


DS_BEGIN(SMCullTrianglesDS, rg::RGDescriptorSetState<SMCullTrianglesDS>)
	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<GPUWorkloadID>),						u_meshletWorkloads)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<GPUWorkloadID>),						u_triangleWorkloads)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<StaticMeshIndirectDrawCallData>),	u_drawTrianglesParams)
DS_END();


DS_BEGIN(SMIndirectRenderTrianglesDS, rg::RGDescriptorSetState<SMIndirectRenderTrianglesDS>)
	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<GPUWorkloadID>),		u_triangleWorkloads)
DS_END();


BEGIN_RG_NODE_PARAMETERS_STRUCT(StaticMeshIndirectBatchDrawParams)
	RG_BUFFER_VIEW(batchDrawCommandsBuffer, rg::ERGBufferAccess::Read, rhi::EPipelineStage::DrawIndirect)
END_RG_NODE_PARAMETERS_STRUCT();


struct StaticMeshBatchRenderingData
{
	rg::RGBufferViewHandle visibleInstancesDispatchParamsBuffer;
	rg::RGBufferViewHandle submeshesWorkloadsDispatchArgsBuffer;
	rg::RGBufferViewHandle meshletsWorkloadsDispatchArgsBuffer;
	rg::RGBufferViewHandle drawTrianglesBatchArgsBuffer;
	
	lib::SharedPtr<StaticMeshBatchDS>			batchDS;
	
	lib::SharedPtr<SMCullInstancesDS>			cullInstancesDS;
	lib::SharedPtr<SMCullSubmeshesDS>			cullSubmeshesDS;
	lib::SharedPtr<SMCullMeshletsDS>			cullMeshletsDS;
	lib::SharedPtr<SMCullTrianglesDS>			cullTrianglesDS;
	lib::SharedPtr<SMIndirectRenderTrianglesDS>	indirectRenderTrianglesDS;

	Uint32 batchedInstancesNum;
};


struct StaticMeshBatchesViewData
{
	lib::SharedPtr<SMProcessBatchForViewDS> viewDS;
	lib::DynamicArray<StaticMeshBatchRenderingData> batches;
};

namespace utils
{

StaticMeshBatchRenderingData CreateBatchRenderingData(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, const StaticMeshBatch& batch)
{
	SPT_PROFILER_FUNCTION();
	
	StaticMeshBatchRenderingData batchRenderingData;

	// Create GPU batch data

	SMGPUBatchData gpuBatchData;
	gpuBatchData.elementsNum = static_cast<Uint32>(batch.batchElements.size());

	// Create Batch elements buffer

	const Uint64 batchDataSize = sizeof(StaticMeshBatchElement) * batch.batchElements.size();
	const rhi::BufferDefinition batchBufferDef(batchDataSize, rhi::EBufferUsage::Storage);
	const rhi::RHIAllocationInfo batchBufferAllocation(rhi::EMemoryUsage::CPUToGPU);
	const lib::SharedRef<rdr::Buffer> batchBuffer = rdr::ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME("StaticMeshesBatch"), batchBufferDef, batchBufferAllocation);

	gfx::UploadDataToBuffer(batchBuffer, 0, reinterpret_cast<const Byte*>(batch.batchElements.data()), batchDataSize);

	batchRenderingData.batchDS = rdr::ResourcesManager::CreateDescriptorSetState<StaticMeshBatchDS>(RENDERER_RESOURCE_NAME("StaticMeshesBatchDS"));
	batchRenderingData.batchDS->u_batchElements = batchBuffer->CreateFullView();
	batchRenderingData.batchDS->u_batchData = gpuBatchData;

	batchRenderingData.batchedInstancesNum = static_cast<Uint32>(batch.batchElements.size());

	const rhi::EBufferUsage indirectBuffersUsageFlags = lib::Flags(rhi::EBufferUsage::Storage, rhi::EBufferUsage::Indirect, rhi::EBufferUsage::TransferDst);
	const rhi::RHIAllocationInfo batchBuffersAllocation(rhi::EMemoryUsage::GPUOnly);
	
	// Create workloads buffers
	
	const rhi::BufferDefinition visibleInstancesBufferDef(sizeof(Uint16) * batch.batchElements.size(), rhi::EBufferUsage::Storage);
	const rg::RGBufferViewHandle visibleInstancesBuffer = graphBuilder.CreateBufferView(RG_DEBUG_NAME("SMVisibleInstances"), visibleInstancesBufferDef, batchBuffersAllocation);

	const rhi::BufferDefinition submeshWorkloadsBufferDef(sizeof(GPUWorkloadID) * batch.maxSubmeshesNum, rhi::EBufferUsage::Storage);
	const rg::RGBufferViewHandle submeshWorkloadsBuffer = graphBuilder.CreateBufferView(RG_DEBUG_NAME("SMSubmeshWorkloads"), submeshWorkloadsBufferDef, batchBuffersAllocation);

	const rhi::BufferDefinition meshletWorkloadsBufferDef(sizeof(GPUWorkloadID) * batch.maxMeshletsNum, rhi::EBufferUsage::Storage);
	const rg::RGBufferViewHandle meshletWorkloadsBuffer = graphBuilder.CreateBufferView(RG_DEBUG_NAME("SMMeshletWorkloads"), meshletWorkloadsBufferDef, batchBuffersAllocation);

	const rhi::BufferDefinition triangleWorkloadsBufferDef(sizeof(GPUWorkloadID) * batch.maxTrianglesNum, rhi::EBufferUsage::Storage);
	const rg::RGBufferViewHandle triangleWorkloadsBuffer = graphBuilder.CreateBufferView(RG_DEBUG_NAME("SMTriangleWorkloads"), triangleWorkloadsBufferDef, batchBuffersAllocation);

	// Create indirect parameters buffers

	const Uint64 indirectDispatchParamsSize = 3 * sizeof(Uint32);
	const Uint64 indirectDrawParamsSize = sizeof(StaticMeshIndirectDrawCallData);

	const rhi::BufferDefinition indirectDispatchesParamsBufferDef(indirectDispatchParamsSize, indirectBuffersUsageFlags);
	const rhi::BufferDefinition indirectDrawsParamsBufferDef(indirectDrawParamsSize, indirectBuffersUsageFlags);

	const rg::RGBufferViewHandle dispatchInstancesParamsBuffer = graphBuilder.CreateBufferView(RG_DEBUG_NAME("DispatchInstancesParams"), indirectDispatchesParamsBufferDef, batchBuffersAllocation);
	graphBuilder.FillBuffer(RG_DEBUG_NAME("SetDefaultDispatchInstancesParams"), dispatchInstancesParamsBuffer, 0, indirectDispatchParamsSize, 0);

	const rg::RGBufferViewHandle dispatchSubmeshesParamsBuffer = graphBuilder.CreateBufferView(RG_DEBUG_NAME("DispatchSubmeshesParams"), indirectDispatchesParamsBufferDef, batchBuffersAllocation);
	graphBuilder.FillBuffer(RG_DEBUG_NAME("SetDefaultDispatchSubmeshesParams"), dispatchSubmeshesParamsBuffer, 0, indirectDispatchParamsSize, 0);

	const rg::RGBufferViewHandle dispatchMeshletsParamsBuffer = graphBuilder.CreateBufferView(RG_DEBUG_NAME("DispatchMeshletsParams"), indirectDispatchesParamsBufferDef, batchBuffersAllocation);
	graphBuilder.FillBuffer(RG_DEBUG_NAME("SetDefaultDispatchMeshletsParams"), dispatchMeshletsParamsBuffer, 0, indirectDispatchParamsSize, 0);

	const rg::RGBufferViewHandle drawTrianglesBatchParamsBuffer = graphBuilder.CreateBufferView(RG_DEBUG_NAME("DrawTrianglesBatchParams"), indirectDrawsParamsBufferDef, batchBuffersAllocation);
	graphBuilder.FillBuffer(RG_DEBUG_NAME("SetDefaultDrawTrianglesParams"), drawTrianglesBatchParamsBuffer, 0, indirectDrawParamsSize, 0);
	
	batchRenderingData.visibleInstancesDispatchParamsBuffer	= dispatchInstancesParamsBuffer;
	batchRenderingData.submeshesWorkloadsDispatchArgsBuffer = dispatchSubmeshesParamsBuffer;
	batchRenderingData.meshletsWorkloadsDispatchArgsBuffer	= dispatchMeshletsParamsBuffer;
	batchRenderingData.drawTrianglesBatchArgsBuffer			= drawTrianglesBatchParamsBuffer;

	// Create descriptor set states

	const lib::SharedRef<SMCullInstancesDS> cullInstancesDS = rdr::ResourcesManager::CreateDescriptorSetState<SMCullInstancesDS>(RENDERER_RESOURCE_NAME("BatchCullInstancesDS"));
	cullInstancesDS->u_visibleInstancesIndices	= visibleInstancesBuffer;
	cullInstancesDS->u_dispatchInstancesParams	= dispatchInstancesParamsBuffer;

	const lib::SharedRef<SMCullSubmeshesDS> cullSubmeshesDS = rdr::ResourcesManager::CreateDescriptorSetState<SMCullSubmeshesDS>(RENDERER_RESOURCE_NAME("BatchCullSubmeshesDS"));
	cullSubmeshesDS->u_visibleInstancesIndices	= visibleInstancesBuffer;
	cullSubmeshesDS->u_submeshWorkloads			= submeshWorkloadsBuffer;
	cullSubmeshesDS->u_dispatchSubmeshesParams	= dispatchSubmeshesParamsBuffer;

	const lib::SharedRef<SMCullMeshletsDS> cullMeshletsDS = rdr::ResourcesManager::CreateDescriptorSetState<SMCullMeshletsDS>(RENDERER_RESOURCE_NAME("BatchCullMeshletsDS"));
	cullMeshletsDS->u_submeshWorkloads			= submeshWorkloadsBuffer;
	cullMeshletsDS->u_meshletWorkloads			= meshletWorkloadsBuffer;
	cullMeshletsDS->u_dispatchMeshletsParams	= dispatchMeshletsParamsBuffer;

	const lib::SharedRef<SMCullTrianglesDS> cullTrianglesDS = rdr::ResourcesManager::CreateDescriptorSetState<SMCullTrianglesDS>(RENDERER_RESOURCE_NAME("BatchCullTrianglesDS"));
	cullTrianglesDS->u_meshletWorkloads			= meshletWorkloadsBuffer;
	cullTrianglesDS->u_triangleWorkloads		= triangleWorkloadsBuffer;
	cullTrianglesDS->u_drawTrianglesParams		= drawTrianglesBatchParamsBuffer;

	const lib::SharedRef<SMIndirectRenderTrianglesDS> indirectRenderTrianglesDS = rdr::ResourcesManager::CreateDescriptorSetState<SMIndirectRenderTrianglesDS>(RENDERER_RESOURCE_NAME("BatchIndirectRenderTrianglesDS"));
	indirectRenderTrianglesDS->u_triangleWorkloads = triangleWorkloadsBuffer;

	batchRenderingData.cullInstancesDS				= cullInstancesDS;
	batchRenderingData.cullSubmeshesDS				= cullSubmeshesDS;
	batchRenderingData.cullMeshletsDS				= cullMeshletsDS;
	batchRenderingData.cullTrianglesDS				= cullTrianglesDS;
	batchRenderingData.indirectRenderTrianglesDS	= indirectRenderTrianglesDS;

	return batchRenderingData;
}

} // utils

StaticMeshesRenderSystem::StaticMeshesRenderSystem()
{
	m_supportedStages = ERenderStage::ForwardOpaque;

	{
		sc::ShaderCompilationSettings compilationSettings;
		compilationSettings.AddShaderToCompile(sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "CullInstancesCS"));
		const rdr::ShaderID cullInstancesShader = rdr::ResourcesManager::CreateShader("StaticMeshes/StaticMesh_CullInstances.hlsl", compilationSettings);
		cullInstancesPipeline = rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("StaticMesh_CullInstancesPipeline"), cullInstancesShader);
	}

	{
		sc::ShaderCompilationSettings compilationSettings;
		compilationSettings.AddShaderToCompile(sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "CullSubmeshesCS"));
		const rdr::ShaderID cullSubmeshesShader = rdr::ResourcesManager::CreateShader("StaticMeshes/StaticMesh_CullSubmeshes.hlsl", compilationSettings);
		cullSubmeshesPipeline = rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("StaticMesh_CullSubmeshesPipeline"), cullSubmeshesShader);
	}
	
	{
		sc::ShaderCompilationSettings compilationSettings;
		compilationSettings.AddShaderToCompile(sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "CullMeshletsCS"));
		const rdr::ShaderID cullMeshletsShader = rdr::ResourcesManager::CreateShader("StaticMeshes/StaticMesh_CullMeshlets.hlsl", compilationSettings);
		cullMeshletsPipeline = rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("StaticMesh_CullMeshletsPipeline"), cullMeshletsShader);
	}

	{
		sc::ShaderCompilationSettings compilationSettings;
		compilationSettings.AddShaderToCompile(sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "CullTrianglesCS"));
		const rdr::ShaderID cullTrianglesShader = rdr::ResourcesManager::CreateShader("StaticMeshes/StaticMesh_CullTriangles.hlsl", compilationSettings);
		cullTrianglesPipeline = rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("StaticMesh_CullTrianglesPipeline"), cullTrianglesShader);
	}

	{
		const RenderTargetFormatsDef forwardOpaqueRTFormats = ForwardOpaqueRenderStage::GetRenderTargetFormats();

		sc::ShaderCompilationSettings compilationSettings;
		compilationSettings.AddShaderToCompile(sc::ShaderStageCompilationDef(rhi::EShaderStage::Vertex, "StaticMeshVS"));
		compilationSettings.AddShaderToCompile(sc::ShaderStageCompilationDef(rhi::EShaderStage::Fragment, "StaticMeshFS"));
		const rdr::ShaderID generateGBufferShader = rdr::ResourcesManager::CreateShader("StaticMeshes/StaticMesh_ForwardOpaqueShading.hlsl", compilationSettings);

		rhi::GraphicsPipelineDefinition pipelineDef;
		pipelineDef.primitiveTopology = rhi::EPrimitiveTopology::TriangleList;
		pipelineDef.renderTargetsDefinition.depthRTDefinition.format = forwardOpaqueRTFormats.depthRTFormat;
		for (const rhi::EFragmentFormat format : forwardOpaqueRTFormats.colorRTFormats)
		{
			pipelineDef.renderTargetsDefinition.colorRTsDefinition.emplace_back(rhi::ColorRenderTargetDefinition(format));
		}

		forwadOpaqueShadingPipeline = rdr::ResourcesManager::CreateGfxPipeline(RENDERER_RESOURCE_NAME("StaticMesh_ForwardOpaqueShading"), pipelineDef, generateGBufferShader);
	}
}

void StaticMeshesRenderSystem::RenderPerView(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec)
{
	SPT_PROFILER_FUNCTION();

	Super::RenderPerView(graphBuilder, renderScene, viewSpec);

	const StaticMeshPrimitivesSystem& staticMeshPrimsSystem = renderScene.GetPrimitivesSystemChecked<StaticMeshPrimitivesSystem>();

	const StaticMeshBatch batch = staticMeshPrimsSystem.BuildBatchForView(viewSpec.GetRenderView());

	// We don't need array here right now, but it's made for future use, as we'll for sure want to have more than one batch
	lib::DynamicArray<StaticMeshBatchRenderingData> batches;

	if (batch.IsValid())
	{
		const StaticMeshBatchRenderingData batchRenderingData = utils::CreateBatchRenderingData(graphBuilder, renderScene, batch);
		batches.emplace_back(batchRenderingData);
	}

	if (!batches.empty())
	{
		StaticMeshBatchesViewData& viewBatches = viewSpec.GetData().Create<StaticMeshBatchesViewData>();
		viewBatches.batches = std::move(batches);

		const RenderView& renderView = viewSpec.GetRenderView();

		const lib::SharedRef<SMProcessBatchForViewDS> viewDS = rdr::ResourcesManager::CreateDescriptorSetState<SMProcessBatchForViewDS>(RENDERER_RESOURCE_NAME("SMViewDS"));
		viewDS->u_sceneView		= renderView.GenerateViewData();
		viewDS->u_cullingData	= renderView.GenerateCullingData(viewDS->u_sceneView.Get());
		
		viewBatches.viewDS = viewDS;
	
		const rg::BindDescriptorSetsScope staticMeshCullingDSScope(graphBuilder,
																   rg::BindDescriptorSets(lib::Ref(StaticMeshUnifiedData::Get().GetUnifiedDataDS()),
																						  renderScene.GetRenderSceneDS(),
																						  viewDS));


		for (const StaticMeshBatchRenderingData& batchRenderingData : viewBatches.batches)
		{
			const rg::BindDescriptorSetsScope staticMeshBatchDSScope(graphBuilder, rg::BindDescriptorSets(lib::Ref(batchRenderingData.batchDS)));

			const Uint32 dispatchGroupsNum = math::Utils::RoundUp<Uint32>(batchRenderingData.batchedInstancesNum, 64) / 64;

			graphBuilder.Dispatch(RG_DEBUG_NAME("SM Cull Instances"),
								  cullInstancesPipeline,
								  math::Vector3u(dispatchGroupsNum, 1, 1),
								  rg::BindDescriptorSets(lib::Ref(batchRenderingData.cullInstancesDS)));
		}

		for (const StaticMeshBatchRenderingData& batchRenderingData : viewBatches.batches)
		{
			const rg::BindDescriptorSetsScope staticMeshBatchDSScope(graphBuilder, rg::BindDescriptorSets(lib::Ref(batchRenderingData.batchDS)));

			graphBuilder.DispatchIndirect(RG_DEBUG_NAME("SM Cull Submeshes"),
										  cullSubmeshesPipeline,
										  batchRenderingData.visibleInstancesDispatchParamsBuffer,
										  0,
										  rg::BindDescriptorSets(lib::Ref(batchRenderingData.cullSubmeshesDS)));
		}

		for (const StaticMeshBatchRenderingData& batchRenderingData : viewBatches.batches)
		{
			const rg::BindDescriptorSetsScope staticMeshBatchDSScope(graphBuilder, rg::BindDescriptorSets(lib::Ref(batchRenderingData.batchDS)));

			graphBuilder.DispatchIndirect(RG_DEBUG_NAME("SM Cull Meshlets"),
										  cullMeshletsPipeline,
										  batchRenderingData.submeshesWorkloadsDispatchArgsBuffer,
										  0,
										  rg::BindDescriptorSets(lib::Ref(batchRenderingData.cullMeshletsDS)));
		}

		for (const StaticMeshBatchRenderingData& batchRenderingData : viewBatches.batches)
		{
			const rg::BindDescriptorSetsScope staticMeshBatchDSScope(graphBuilder, rg::BindDescriptorSets(lib::Ref(batchRenderingData.batchDS)));

			graphBuilder.DispatchIndirect(RG_DEBUG_NAME("SM Cull Triangles"),
										  cullTrianglesPipeline,
										  batchRenderingData.meshletsWorkloadsDispatchArgsBuffer,
										  0,
										  rg::BindDescriptorSets(lib::Ref(batchRenderingData.cullTrianglesDS),
																 lib::Ref(GeometryManager::Get().GetGeometryDSState())));
		}


		if (viewSpec.SupportsStage(ERenderStage::ForwardOpaque))
		{
			RenderStageEntries& basePassStageEntries = viewSpec.GetRenderStageEntries(ERenderStage::ForwardOpaque);

			basePassStageEntries.GetOnRenderStage().AddRawMember(this, &StaticMeshesRenderSystem::RenderMeshesPerView);
		}
	}
}

void StaticMeshesRenderSystem::RenderMeshesPerView(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& context)
{
	SPT_PROFILER_FUNCTION();

	const StaticMeshBatchesViewData& viewBatches = viewSpec.GetData().Get<StaticMeshBatchesViewData>();

	const rg::BindDescriptorSetsScope staticMeshRenderingDSScope(graphBuilder,
																 rg::BindDescriptorSets(lib::Ref(StaticMeshUnifiedData::Get().GetUnifiedDataDS()),
																						renderScene.GetRenderSceneDS(),
																						lib::Ref(GeometryManager::Get().GetGeometryDSState()),
																						lib::Ref(viewBatches.viewDS)));

	for (const StaticMeshBatchRenderingData& batch : viewBatches.batches)
	{
		const rg::BindDescriptorSetsScope staticMeshBatchDSScope(graphBuilder, rg::BindDescriptorSets(lib::Ref(batch.batchDS)));

		StaticMeshIndirectBatchDrawParams drawParams;
		drawParams.batchDrawCommandsBuffer = batch.drawTrianglesBatchArgsBuffer;

		const lib::SharedRef<GeometryDS> unifiedGeometryDS = lib::Ref(GeometryManager::Get().GetGeometryDSState());

		graphBuilder.AddSubpass(RG_DEBUG_NAME("Render Static Meshes Batch"),
								rg::BindDescriptorSets(lib::Ref(batch.indirectRenderTrianglesDS)),
								std::tie(drawParams),
								[drawParams, &viewSpec, this](const lib::SharedRef<rdr::RenderContext>& renderContext, rdr::CommandRecorder& recorder)
								{
									recorder.BindGraphicsPipeline(forwadOpaqueShadingPipeline);

									const math::Vector2u renderingArea = viewSpec.GetRenderView().GetRenderingResolution();

									recorder.SetViewport(math::AlignedBox2f(math::Vector2f(0.f, 0.f), renderingArea.cast<Real32>()), 0.f, 1.f);
									recorder.SetScissor(math::AlignedBox2u(math::Vector2u(0, 0), renderingArea));

									const rdr::BufferView& drawsBufferView = drawParams.batchDrawCommandsBuffer->GetBufferViewInstance();
									recorder.DrawIndirect(drawsBufferView, 0, sizeof(StaticMeshIndirectDrawCallData), 1);
								});
	}
}

} // spt::rsc
