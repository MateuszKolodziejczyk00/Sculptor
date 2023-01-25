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

//////////////////////////////////////////////////////////////////////////////////////////////////
// Common Data ===================================================================================

BEGIN_SHADER_STRUCT(, SMIndirectDrawCallData)
	SHADER_STRUCT_FIELD(Uint32, vertexCount)
	SHADER_STRUCT_FIELD(Uint32, instanceCount)
	SHADER_STRUCT_FIELD(Uint32, firstVertex)
	SHADER_STRUCT_FIELD(Uint32, firstInstance)
END_SHADER_STRUCT();


BEGIN_SHADER_STRUCT(, SMGPUWorkloadID)
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


struct SMRenderingViewData
{
	lib::SharedPtr<SMProcessBatchForViewDS> viewDS;
};


struct StaticMeshesVisibleLastFrame
{
	struct InstancesInfo
	{
		lib::SharedPtr<rdr::Buffer> instancesBuffer;
		lib::SharedPtr<rdr::Buffer> instancesDispatchParamsBuffer;

		Uint32 maxInstancesNum;
		Uint32 maxSubmeshesNum;
	};

	lib::DynamicArray<InstancesInfo> visibleInstances;
};


//////////////////////////////////////////////////////////////////////////////////////////////////
// Opaque Forward ================================================================================

DS_BEGIN(SMCullInstancesDS, rg::RGDescriptorSetState<SMCullInstancesDS>)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<StaticMeshBatchElement>),	u_visibleInstances)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<Uint32>),					u_dispatchInstancesParams)
DS_END();


DS_BEGIN(SMCullSubmeshesDS, rg::RGDescriptorSetState<SMCullSubmeshesDS>)
	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<StaticMeshBatchElement>),	u_visibleInstances)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<SMGPUWorkloadID>),		u_submeshWorkloads)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<Uint32>),				u_dispatchSubmeshesParams)
DS_END();


DS_BEGIN(SMCullMeshletsDS, rg::RGDescriptorSetState<SMCullMeshletsDS>)
	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<StaticMeshBatchElement>),	u_visibleInstances)
	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<SMGPUWorkloadID>),			u_submeshWorkloads)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<SMGPUWorkloadID>),		u_meshletWorkloads)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<Uint32>),				u_dispatchMeshletsParams)
DS_END();


DS_BEGIN(SMCullTrianglesDS, rg::RGDescriptorSetState<SMCullTrianglesDS>)
	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<StaticMeshBatchElement>),				u_visibleInstances)
	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<SMGPUWorkloadID>),						u_meshletWorkloads)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<SMGPUWorkloadID>),					u_triangleWorkloads)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<SMIndirectDrawCallData>),	u_drawTrianglesParams)
DS_END();


DS_BEGIN(SMIndirectRenderTrianglesDS, rg::RGDescriptorSetState<SMIndirectRenderTrianglesDS>)
	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<StaticMeshBatchElement>),	u_visibleInstances)
	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<SMGPUWorkloadID>),			u_triangleWorkloads)
DS_END();


BEGIN_RG_NODE_PARAMETERS_STRUCT(StaticMeshIndirectBatchDrawParams)
	RG_BUFFER_VIEW(batchDrawCommandsBuffer, rg::ERGBufferAccess::Read, rhi::EPipelineStage::DrawIndirect)
END_RG_NODE_PARAMETERS_STRUCT();

struct SMForwardOpaqueBatch
{
	lib::SharedPtr<rdr::Buffer> visibleInstancesBuffer;
	lib::SharedPtr<rdr::Buffer> visibleInstancesDispatchParamsBuffer;

	rg::RGBufferViewHandle visibleInstancesDispatchParamsRGBuffer;
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
	Uint32 batchedSubmeshesNum;
};


struct SMOpaqueForwardBatches
{
	lib::DynamicArray<SMForwardOpaqueBatch> batches;
};

//////////////////////////////////////////////////////////////////////////////////////////////////
// DepthPrepass ==================================================================================

BEGIN_SHADER_STRUCT(, SMDepthPrepassDrawCallData)
	/* Vulkan command data */
	SHADER_STRUCT_FIELD(Uint32, vertexCount)
	SHADER_STRUCT_FIELD(Uint32, instanceCount)
	SHADER_STRUCT_FIELD(Uint32, firstVertex)
	SHADER_STRUCT_FIELD(Uint32, firstInstance)
	
	/* Custom Data */
	SHADER_STRUCT_FIELD(Uint32, batchElementIdx)
	SHADER_STRUCT_FIELD(Uint32, submeshGlobalIdx)
	SHADER_STRUCT_FIELD(Uint32, padding0)
	SHADER_STRUCT_FIELD(Uint32, padding1)
END_SHADER_STRUCT();


DS_BEGIN(SMDepthPrepassCullInstancesDS, rg::RGDescriptorSetState<SMDepthPrepassCullInstancesDS>)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<SMDepthPrepassDrawCallData>),	u_drawCommands)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<Uint32>),						u_drawsCount)
	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<Uint32>),							u_validBatchElementsNum)
DS_END();


DS_BEGIN(SMDepthPrepassDrawInstancesDS, rg::RGDescriptorSetState<SMDepthPrepassDrawInstancesDS>)
	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<SMDepthPrepassDrawCallData>),	u_drawCommands)
DS_END();


struct SMDepthPrepassBatch
{
	rg::RGBufferViewHandle visibleInstancesDrawParamsBuffer;
	
	lib::SharedPtr<StaticMeshBatchDS>				batchDS;
	
	lib::SharedPtr<SMDepthPrepassCullInstancesDS>	cullInstancesDS;
	lib::SharedPtr<SMDepthPrepassDrawInstancesDS>	drawInstancesDS;

	Uint32 batchedInstancesNum;
};


struct SMDepthPrepassBatches
{
	lib::DynamicArray<SMDepthPrepassBatch> batches;
};

//////////////////////////////////////////////////////////////////////////////////////////////////
// Utilities =====================================================================================

namespace utils
{

SMForwardOpaqueBatch CreateForwardOpaqueBatch(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, const StaticMeshBatchDefinition& batchDef)
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

	batch.batchedInstancesNum = static_cast<Uint32>(batchDef.batchElements.size());
	batch.batchedSubmeshesNum = static_cast<Uint32>(batchDef.maxSubmeshesNum);

	const rhi::EBufferUsage indirectBuffersUsageFlags = lib::Flags(rhi::EBufferUsage::Storage, rhi::EBufferUsage::Indirect, rhi::EBufferUsage::TransferDst);
	const rhi::RHIAllocationInfo batchBuffersAllocation(rhi::EMemoryUsage::GPUOnly);
	
	// Create workloads buffers

	// visible instances buffer is not created using render graph because it's lifetime must be longer than 1 frame
	// That's because we will use this buffer to render depth prepass next frame
	const rhi::BufferDefinition visibleInstancesBufferDef(sizeof(StaticMeshBatchElement) * batchDef.batchElements.size(), rhi::EBufferUsage::Storage);
	const lib::SharedRef<rdr::Buffer> visibleInstancesBuffer = rdr::ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME("SMVisibleInstances"), visibleInstancesBufferDef, batchBuffersAllocation);

	const rhi::BufferDefinition submeshWorkloadsBufferDef(sizeof(SMGPUWorkloadID) * batchDef.maxSubmeshesNum, rhi::EBufferUsage::Storage);
	const rg::RGBufferViewHandle submeshWorkloadsBuffer = graphBuilder.CreateBufferView(RG_DEBUG_NAME("SMSubmeshWorkloads"), submeshWorkloadsBufferDef, batchBuffersAllocation);

	const rhi::BufferDefinition meshletWorkloadsBufferDef(sizeof(SMGPUWorkloadID) * batchDef.maxMeshletsNum, rhi::EBufferUsage::Storage);
	const rg::RGBufferViewHandle meshletWorkloadsBuffer = graphBuilder.CreateBufferView(RG_DEBUG_NAME("SMMeshletWorkloads"), meshletWorkloadsBufferDef, batchBuffersAllocation);

	const rhi::BufferDefinition triangleWorkloadsBufferDef(sizeof(SMGPUWorkloadID) * batchDef.maxTrianglesNum, rhi::EBufferUsage::Storage);
	const rg::RGBufferViewHandle triangleWorkloadsBuffer = graphBuilder.CreateBufferView(RG_DEBUG_NAME("SMTriangleWorkloads"), triangleWorkloadsBufferDef, batchBuffersAllocation);

	// Create indirect parameters buffers

	const Uint64 indirectDispatchParamsSize = 3 * sizeof(Uint32);
	const Uint64 indirectDrawParamsSize = sizeof(SMIndirectDrawCallData);

	const rhi::BufferDefinition indirectDispatchesParamsBufferDef(indirectDispatchParamsSize, indirectBuffersUsageFlags);
	const rhi::BufferDefinition indirectDrawsParamsBufferDef(indirectDrawParamsSize, indirectBuffersUsageFlags);

	const rg::RGBufferViewHandle dispatchSubmeshesParamsBuffer = graphBuilder.CreateBufferView(RG_DEBUG_NAME("DispatchSubmeshesParams"), indirectDispatchesParamsBufferDef, batchBuffersAllocation);
	graphBuilder.FillBuffer(RG_DEBUG_NAME("SetDefaultDispatchSubmeshesParams"), dispatchSubmeshesParamsBuffer, 0, indirectDispatchParamsSize, 0);

	const rg::RGBufferViewHandle dispatchMeshletsParamsBuffer = graphBuilder.CreateBufferView(RG_DEBUG_NAME("DispatchMeshletsParams"), indirectDispatchesParamsBufferDef, batchBuffersAllocation);
	graphBuilder.FillBuffer(RG_DEBUG_NAME("SetDefaultDispatchMeshletsParams"), dispatchMeshletsParamsBuffer, 0, indirectDispatchParamsSize, 0);

	const rg::RGBufferViewHandle drawTrianglesBatchParamsBuffer = graphBuilder.CreateBufferView(RG_DEBUG_NAME("DrawTrianglesBatchParams"), indirectDrawsParamsBufferDef, batchBuffersAllocation);
	graphBuilder.FillBuffer(RG_DEBUG_NAME("SetDefaultDrawTrianglesParams"), drawTrianglesBatchParamsBuffer, 0, indirectDrawParamsSize, 0);

	// This also lives longer than frame, so it's not created using render graph
	const lib::SharedRef<rdr::Buffer> visibleInstancesDispatchParamsBuffer = rdr::ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME("DispatchInstancesParams"), indirectDispatchesParamsBufferDef, batchBuffersAllocation);
	const rg::RGBufferViewHandle dispatchInstancesParamsBuffer = graphBuilder.AcquireExternalBufferView(visibleInstancesDispatchParamsBuffer->CreateFullView());
	graphBuilder.FillBuffer(RG_DEBUG_NAME("SetDefaultDrawTrianglesParams"), dispatchInstancesParamsBuffer, 0, indirectDispatchParamsSize, 0);

	batch.visibleInstancesBuffer					= visibleInstancesBuffer;
	batch.visibleInstancesDispatchParamsBuffer		= visibleInstancesDispatchParamsBuffer;

	batch.visibleInstancesDispatchParamsRGBuffer	= dispatchInstancesParamsBuffer;
	batch.submeshesWorkloadsDispatchArgsBuffer		= dispatchSubmeshesParamsBuffer;
	batch.meshletsWorkloadsDispatchArgsBuffer		= dispatchMeshletsParamsBuffer;
	batch.drawTrianglesBatchArgsBuffer				= drawTrianglesBatchParamsBuffer;

	const rg::RGBufferViewHandle visibleInstancesRGBuffer = graphBuilder.AcquireExternalBufferView(visibleInstancesBuffer->CreateFullView());

	// Create descriptor set states

	const lib::SharedRef<SMCullInstancesDS> cullInstancesDS = rdr::ResourcesManager::CreateDescriptorSetState<SMCullInstancesDS>(RENDERER_RESOURCE_NAME("BatchCullInstancesDS"));
	cullInstancesDS->u_visibleInstances			= visibleInstancesRGBuffer;
	cullInstancesDS->u_dispatchInstancesParams	= dispatchInstancesParamsBuffer;

	const lib::SharedRef<SMCullSubmeshesDS> cullSubmeshesDS = rdr::ResourcesManager::CreateDescriptorSetState<SMCullSubmeshesDS>(RENDERER_RESOURCE_NAME("BatchCullSubmeshesDS"));
	cullSubmeshesDS->u_visibleInstances			= visibleInstancesRGBuffer;
	cullSubmeshesDS->u_submeshWorkloads			= submeshWorkloadsBuffer;
	cullSubmeshesDS->u_dispatchSubmeshesParams	= dispatchSubmeshesParamsBuffer;

	const lib::SharedRef<SMCullMeshletsDS> cullMeshletsDS = rdr::ResourcesManager::CreateDescriptorSetState<SMCullMeshletsDS>(RENDERER_RESOURCE_NAME("BatchCullMeshletsDS"));
	cullMeshletsDS->u_visibleInstances			= visibleInstancesRGBuffer;
	cullMeshletsDS->u_submeshWorkloads			= submeshWorkloadsBuffer;
	cullMeshletsDS->u_meshletWorkloads			= meshletWorkloadsBuffer;
	cullMeshletsDS->u_dispatchMeshletsParams	= dispatchMeshletsParamsBuffer;

	const lib::SharedRef<SMCullTrianglesDS> cullTrianglesDS = rdr::ResourcesManager::CreateDescriptorSetState<SMCullTrianglesDS>(RENDERER_RESOURCE_NAME("BatchCullTrianglesDS"));
	cullTrianglesDS->u_visibleInstances			= visibleInstancesRGBuffer;
	cullTrianglesDS->u_meshletWorkloads			= meshletWorkloadsBuffer;
	cullTrianglesDS->u_triangleWorkloads		= triangleWorkloadsBuffer;
	cullTrianglesDS->u_drawTrianglesParams		= drawTrianglesBatchParamsBuffer;

	const lib::SharedRef<SMIndirectRenderTrianglesDS> indirectRenderTrianglesDS = rdr::ResourcesManager::CreateDescriptorSetState<SMIndirectRenderTrianglesDS>(RENDERER_RESOURCE_NAME("BatchIndirectRenderTrianglesDS"));
	indirectRenderTrianglesDS->u_visibleInstances	= visibleInstancesRGBuffer;
	indirectRenderTrianglesDS->u_triangleWorkloads	= triangleWorkloadsBuffer;

	batch.cullInstancesDS				= cullInstancesDS;
	batch.cullSubmeshesDS				= cullSubmeshesDS;
	batch.cullMeshletsDS				= cullMeshletsDS;
	batch.cullTrianglesDS				= cullTrianglesDS;
	batch.indirectRenderTrianglesDS		= indirectRenderTrianglesDS;

	return batch;
}

SMDepthPrepassBatch CreateDepthPrepassBatch(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, const StaticMeshesVisibleLastFrame::InstancesInfo& instances)
{
	SPT_PROFILER_FUNCTION();

	SMDepthPrepassBatch batch;

	// Create batch info
	
	batch.visibleInstancesDrawParamsBuffer = graphBuilder.AcquireExternalBufferView(instances.instancesDispatchParamsBuffer->CreateFullView());

	SMGPUBatchData batchData;
	batchData.elementsNum = instances.maxInstancesNum;

	const lib::SharedRef<StaticMeshBatchDS> batchDS = rdr::ResourcesManager::CreateDescriptorSetState<StaticMeshBatchDS>(RENDERER_RESOURCE_NAME("DepthPrepassBatchDS"));
	batchDS->u_batchElements = instances.instancesBuffer->CreateFullView();
	batchDS->u_batchData = batchData;

	batch.batchDS = batchDS;
	batch.batchedInstancesNum = instances.maxInstancesNum;

	// Create buffers

	const rhi::RHIAllocationInfo batchBuffersAllocInfo(rhi::EMemoryUsage::GPUOnly);

	const rhi::BufferDefinition commandsBufferDef(sizeof(SMDepthPrepassDrawCallData) * instances.maxSubmeshesNum, lib::Flags(rhi::EBufferUsage::Storage, rhi::EBufferUsage::Indirect));
	const rg::RGBufferViewHandle drawCommandsBuffer = graphBuilder.CreateBufferView(RG_DEBUG_NAME("SMDepthPrepassIndirectCommands"), commandsBufferDef, batchBuffersAllocInfo);

	const Uint64 indirectDrawsCountSize = sizeof(Uint32);
	const rhi::BufferDefinition indirectDrawCountBufferDef(indirectDrawsCountSize, lib::Flags(rhi::EBufferUsage::Storage, rhi::EBufferUsage::Indirect, rhi::EBufferUsage::TransferDst));
	const rg::RGBufferViewHandle indirectDrawCountBuffer = graphBuilder.CreateBufferView(RG_DEBUG_NAME("SMDepthPrepassIndirectCommandsCount"), indirectDrawCountBufferDef, batchBuffersAllocInfo);
	graphBuilder.FillBuffer(RG_DEBUG_NAME("SetSMDepthPrepassIndirectCommandsDefaultCount"), indirectDrawCountBuffer, 0, indirectDrawsCountSize, 0);

	// Create Descriptor Sets
	
	const lib::SharedRef<SMDepthPrepassCullInstancesDS> cullInstancesDS = rdr::ResourcesManager::CreateDescriptorSetState<SMDepthPrepassCullInstancesDS>(RENDERER_RESOURCE_NAME("SMDepthPrepassCullInstancesDS"));
	cullInstancesDS->u_drawCommands				= drawCommandsBuffer;
	cullInstancesDS->u_drawsCount				= indirectDrawCountBuffer;
	cullInstancesDS->u_validBatchElementsNum	= rdr::BufferView(lib::Ref(instances.instancesDispatchParamsBuffer), 0, sizeof(Uint32));

	const lib::SharedRef<SMDepthPrepassDrawInstancesDS> drawInstancesDS = rdr::ResourcesManager::CreateDescriptorSetState<SMDepthPrepassDrawInstancesDS>(RENDERER_RESOURCE_NAME("SMDepthPrepassDrawInstancesDS"));
	drawInstancesDS->u_drawCommands = drawCommandsBuffer;

	batch.cullInstancesDS = cullInstancesDS;
	batch.drawInstancesDS = drawInstancesDS;

	return batch;
}

} // utils

//////////////////////////////////////////////////////////////////////////////////////////////////
// StaticMeshesRenderSystem ======================================================================

StaticMeshesRenderSystem::StaticMeshesRenderSystem()
{
	m_supportedStages = lib::Flags(ERenderStage::ForwardOpaque, ERenderStage::DepthPrepass);

	{
		sc::ShaderCompilationSettings compilationSettings;
		compilationSettings.AddShaderToCompile(sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "BuildDrawCommandsCS"));
		const rdr::ShaderID buildDrawCommandsShader = rdr::ResourcesManager::CreateShader("Sculptor/StaticMeshes/StaticMesh_BuildDepthPrepassDrawCommands.hlsl", compilationSettings);
		m_buildDrawCommandsPipeline = rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("StaticMesh_BuildDepthPrepassDrawCommands"), buildDrawCommandsShader);
	}

	{
		sc::ShaderCompilationSettings compilationSettings;
		compilationSettings.AddShaderToCompile(sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "CullInstancesCS"));
		const rdr::ShaderID cullInstancesShader = rdr::ResourcesManager::CreateShader("Sculptor/StaticMeshes/StaticMesh_CullInstances.hlsl", compilationSettings);
		m_cullInstancesPipeline = rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("StaticMesh_CullInstancesPipeline"), cullInstancesShader);
	}

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
		const rdr::ShaderID generateGBufferShader = rdr::ResourcesManager::CreateShader("Sculptor/StaticMeshes/StaticMesh_ForwardOpaqueShading.hlsl", compilationSettings);

		rhi::GraphicsPipelineDefinition pipelineDef;
		pipelineDef.primitiveTopology = rhi::EPrimitiveTopology::TriangleList;
		pipelineDef.renderTargetsDefinition.depthRTDefinition.format = forwardOpaqueRTFormats.depthRTFormat;
		for (const rhi::EFragmentFormat format : forwardOpaqueRTFormats.colorRTFormats)
		{
			pipelineDef.renderTargetsDefinition.colorRTsDefinition.emplace_back(rhi::ColorRenderTargetDefinition(format));
		}

		m_forwadOpaqueShadingPipeline = rdr::ResourcesManager::CreateGfxPipeline(RENDERER_RESOURCE_NAME("StaticMesh_ForwardOpaqueShading"), pipelineDef, generateGBufferShader);
	}
}

void StaticMeshesRenderSystem::RenderPerView(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec)
{
	SPT_PROFILER_FUNCTION();

	Super::RenderPerView(graphBuilder, renderScene, viewSpec);

	const RenderView& renderView = viewSpec.GetRenderView();

	const lib::SharedRef<SMProcessBatchForViewDS> viewDS = rdr::ResourcesManager::CreateDescriptorSetState<SMProcessBatchForViewDS>(RENDERER_RESOURCE_NAME("SMViewDS"));
	viewDS->u_sceneView		= renderView.GenerateViewData();
	viewDS->u_cullingData	= renderView.GenerateCullingData(viewDS->u_sceneView.Get());

	viewSpec.GetData().Create<SMRenderingViewData>(SMRenderingViewData{ std::move(viewDS) });
	
	if (viewSpec.SupportsStage(ERenderStage::DepthPrepass))
	{
		const Bool hasAnyBatches = BuildDepthPrepassBatchesPerView(graphBuilder, renderScene, viewSpec);

		if (hasAnyBatches)
		{
			CullDepthPrepassPerView(graphBuilder, renderScene, viewSpec);

			RenderStageEntries& basePassStageEntries = viewSpec.GetRenderStageEntries(ERenderStage::DepthPrepass);
			basePassStageEntries.GetOnRenderStage().AddRawMember(this, &StaticMeshesRenderSystem::RenderDepthPrepassPerView);
		}
	}

	if (viewSpec.SupportsStage(ERenderStage::ForwardOpaque))
	{
		const Bool hasAnyBatches = BuildForwardOpaueBatchesPerView(graphBuilder, renderScene, viewSpec);

		if (hasAnyBatches)
		{
			CullForwardOpaquePerView(graphBuilder, renderScene, viewSpec);

			RenderStageEntries& basePassStageEntries = viewSpec.GetRenderStageEntries(ERenderStage::ForwardOpaque);
			basePassStageEntries.GetOnRenderStage().AddRawMember(this, &StaticMeshesRenderSystem::RenderForwardOpaquePerView);
		}
	}
}

Bool StaticMeshesRenderSystem::BuildDepthPrepassBatchesPerView(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec)
{
	SPT_PROFILER_FUNCTION();
	
	const RenderView& renderView = viewSpec.GetRenderView();
	RenderSceneEntityHandle viewEntityHandle = renderView.GetViewEntity();
	
	// Get instances visible last frame
	const StaticMeshesVisibleLastFrame* visibleInstances = viewEntityHandle.try_get<StaticMeshesVisibleLastFrame>();
	if (visibleInstances)
	{
		lib::DynamicArray<SMDepthPrepassBatch> batches;

		for (const StaticMeshesVisibleLastFrame::InstancesInfo& instances : visibleInstances->visibleInstances)
		{
			batches.emplace_back(utils::CreateDepthPrepassBatch(graphBuilder, renderScene, instances));
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

void StaticMeshesRenderSystem::CullDepthPrepassPerView(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec)
{
	SPT_PROFILER_FUNCTION();

	const SMDepthPrepassBatches& depthPrepassBatches = viewSpec.GetData().Get<SMDepthPrepassBatches>();

	const SMRenderingViewData& staticMeshRenderingViewData = viewSpec.GetData().Get<SMRenderingViewData>();

	const rg::BindDescriptorSetsScope staticMeshCullingDSScope(graphBuilder,
															   rg::BindDescriptorSets(lib::Ref(StaticMeshUnifiedData::Get().GetUnifiedDataDS()),
																					  renderScene.GetRenderSceneDS(),
																					  lib::Ref(staticMeshRenderingViewData.viewDS)));

	for (const SMDepthPrepassBatch& batch : depthPrepassBatches.batches)
	{
		const rg::BindDescriptorSetsScope staticMeshBatchDSScope(graphBuilder, rg::BindDescriptorSets(lib::Ref(batch.batchDS)));

		const Uint32 dispatchGroupsNum = math::Utils::RoundUp<Uint32>(batch.batchedInstancesNum, 64) / 64;

		graphBuilder.Dispatch(RG_DEBUG_NAME("SM Cull And Build Draw Commands"),
							  m_buildDrawCommandsPipeline,
							  math::Vector3u(dispatchGroupsNum, 1, 1),
							  rg::BindDescriptorSets(lib::Ref(batch.cullInstancesDS)));
	}
}

void StaticMeshesRenderSystem::RenderDepthPrepassPerView(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& context)
{

}

Bool StaticMeshesRenderSystem::BuildForwardOpaueBatchesPerView(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec)
{
	SPT_PROFILER_FUNCTION()

	const StaticMeshPrimitivesSystem& staticMeshPrimsSystem = renderScene.GetPrimitivesSystemChecked<StaticMeshPrimitivesSystem>();

	const StaticMeshBatchDefinition batchDef = staticMeshPrimsSystem.BuildBatchForView(viewSpec.GetRenderView());

	// We don't need array here right now, but it's made for future use, as we'll for sure want to have more than one batch
	lib::DynamicArray<SMForwardOpaqueBatch> batches;

	if (batchDef.IsValid())
	{
		const SMForwardOpaqueBatch batchRenderingData = utils::CreateForwardOpaqueBatch(graphBuilder, renderScene, batchDef);
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
							   StaticMeshesVisibleLastFrame::InstancesInfo instances;
							   instances.instancesBuffer = batchData.visibleInstancesBuffer;
							   instances.instancesDispatchParamsBuffer = batchData.visibleInstancesDispatchParamsBuffer;
							   instances.maxInstancesNum = batchData.batchedInstancesNum;
							   instances.maxSubmeshesNum = batchData.batchedSubmeshesNum;
							   return instances;
						   });
		}

		SMOpaqueForwardBatches& viewBatches = viewSpec.GetData().Create<SMOpaqueForwardBatches>();
		viewBatches.batches = std::move(batches);

		return true;
	}

	return false;
}

void StaticMeshesRenderSystem::CullForwardOpaquePerView(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec)
{
	SPT_PROFILER_FUNCTION();

	const SMOpaqueForwardBatches& forwardOpaqueBatches = viewSpec.GetData().Get<SMOpaqueForwardBatches>();

	const SMRenderingViewData& staticMeshRenderingViewData = viewSpec.GetData().Get<SMRenderingViewData>();

	const rg::BindDescriptorSetsScope staticMeshCullingDSScope(graphBuilder,
															   rg::BindDescriptorSets(lib::Ref(StaticMeshUnifiedData::Get().GetUnifiedDataDS()),
																					  renderScene.GetRenderSceneDS(),
																					  lib::Ref(staticMeshRenderingViewData.viewDS)));


	for (const SMForwardOpaqueBatch& batch : forwardOpaqueBatches.batches)
	{
		const rg::BindDescriptorSetsScope staticMeshBatchDSScope(graphBuilder, rg::BindDescriptorSets(lib::Ref(batch.batchDS)));

		const Uint32 dispatchGroupsNum = math::Utils::RoundUp<Uint32>(batch.batchedInstancesNum, 64) / 64;

		graphBuilder.Dispatch(RG_DEBUG_NAME("SM Cull Instances"),
							  m_cullInstancesPipeline,
							  math::Vector3u(dispatchGroupsNum, 1, 1),
							  rg::BindDescriptorSets(lib::Ref(batch.cullInstancesDS)));
	}

	for (const SMForwardOpaqueBatch& batch : forwardOpaqueBatches.batches)
	{
		const rg::BindDescriptorSetsScope staticMeshBatchDSScope(graphBuilder, rg::BindDescriptorSets(lib::Ref(batch.batchDS)));

		graphBuilder.DispatchIndirect(RG_DEBUG_NAME("SM Cull Submeshes"),
									  m_cullSubmeshesPipeline,
									  batch.visibleInstancesDispatchParamsRGBuffer,
									  0,
									  rg::BindDescriptorSets(lib::Ref(batch.cullSubmeshesDS)));
	}

	for (const SMForwardOpaqueBatch& batch : forwardOpaqueBatches.batches)
	{
		const rg::BindDescriptorSetsScope staticMeshBatchDSScope(graphBuilder, rg::BindDescriptorSets(lib::Ref(batch.batchDS)));

		graphBuilder.DispatchIndirect(RG_DEBUG_NAME("SM Cull Meshlets"),
									  m_cullMeshletsPipeline,
									  batch.submeshesWorkloadsDispatchArgsBuffer,
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

void StaticMeshesRenderSystem::RenderForwardOpaquePerView(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& context)
{
	SPT_PROFILER_FUNCTION();

	const SMOpaqueForwardBatches& forwardOpaqueBatches = viewSpec.GetData().Get<SMOpaqueForwardBatches>();

	const SMRenderingViewData& staticMeshRenderingViewData = viewSpec.GetData().Get<SMRenderingViewData>();

	const rg::BindDescriptorSetsScope staticMeshRenderingDSScope(graphBuilder,
																 rg::BindDescriptorSets(lib::Ref(StaticMeshUnifiedData::Get().GetUnifiedDataDS()),
																						renderScene.GetRenderSceneDS(),
																						lib::Ref(GeometryManager::Get().GetGeometryDSState()),
																						lib::Ref(staticMeshRenderingViewData.viewDS)));

	for (const SMForwardOpaqueBatch& batch : forwardOpaqueBatches.batches)
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
									recorder.BindGraphicsPipeline(m_forwadOpaqueShadingPipeline);

									const math::Vector2u renderingArea = viewSpec.GetRenderView().GetRenderingResolution();

									recorder.SetViewport(math::AlignedBox2f(math::Vector2f(0.f, 0.f), renderingArea.cast<Real32>()), 0.f, 1.f);
									recorder.SetScissor(math::AlignedBox2u(math::Vector2u(0, 0), renderingArea));

									const rdr::BufferView& drawsBufferView = drawParams.batchDrawCommandsBuffer->GetBufferViewInstance();
									recorder.DrawIndirect(drawsBufferView, 0, sizeof(SMIndirectDrawCallData), 1);
								});
	}
}

} // spt::rsc
