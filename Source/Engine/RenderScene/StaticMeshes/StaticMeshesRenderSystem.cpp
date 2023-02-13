#include "StaticMeshesRenderSystem.h"
#include "StaticMeshPrimitivesSystem.h"
#include "ShaderStructs/ShaderStructsMacros.h"
#include "RenderGraphBuilder.h"
#include "View/ViewRenderingSpec.h"
#include "View/RenderView.h"
#include "RenderScene.h"
#include "RGDescriptorSetState.h"
#include "DescriptorSetBindings/RWBufferBinding.h"
#include "DescriptorSetBindings/ConstantBufferBinding.h"
#include "DescriptorSetBindings/SRVTextureBinding.h"
#include "Common/ShaderCompilationInput.h"
#include "GeometryManager.h"
#include "StaticMeshGeometry.h"
#include "Transfers/UploadUtils.h"
#include "SceneRenderer/RenderStages/ForwardOpaqueRenderStage.h"
#include "SceneRenderer/RenderStages/DepthPrepassRenderStage.h"
#include "Materials/MaterialsUnifiedData.h"
#include "SceneRenderer/SceneRendererTypes.h"
#include "SceneRenderer/Parameters/SceneRendererParams.h"

namespace spt::rsc
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// Parameters ====================================================================================

namespace params
{

RendererBoolParameter enableDepthCulling("Enable Depth Culling", { "Geometry", "Static Mesh", "Culling" }, true);

} // params

//////////////////////////////////////////////////////////////////////////////////////////////////
// Common Data ===================================================================================

BEGIN_SHADER_STRUCT(SMIndirectDrawCallData)
	SHADER_STRUCT_FIELD(Uint32, vertexCount)
	SHADER_STRUCT_FIELD(Uint32, instanceCount)
	SHADER_STRUCT_FIELD(Uint32, firstVertex)
	SHADER_STRUCT_FIELD(Uint32, firstInstance)
END_SHADER_STRUCT();


BEGIN_SHADER_STRUCT(SMGPUWorkloadID)
	SHADER_STRUCT_FIELD(Uint32, data1)
END_SHADER_STRUCT();


BEGIN_SHADER_STRUCT(SMGPUBatchData)
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
	struct BatchElementsInfo
	{
		lib::SharedPtr<rdr::Buffer> visibleBatchElementsBuffer;
		lib::SharedPtr<rdr::Buffer> batchElementsDispatchParamsBuffer;

		Uint32 batchedSubmeshesNum;
	};

	lib::DynamicArray<BatchElementsInfo> visibleInstances;
};


//////////////////////////////////////////////////////////////////////////////////////////////////
// Opaque Forward ================================================================================

BEGIN_SHADER_STRUCT(SMDepthCullingParams)
	SHADER_STRUCT_FIELD(math::Vector2f, hiZResolution)
END_SHADER_STRUCT();


DS_BEGIN(SMDepthCullingDS, rg::RGDescriptorSetState<SMDepthCullingDS>)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),										u_hiZTexture)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::LinearMinClampToEdge>),	u_hiZSampler)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableConstantBufferBinding<SMDepthCullingParams>),				u_depthCullingParams)
DS_END();


DS_BEGIN(SMCullSubmeshesDS, rg::RGDescriptorSetState<SMCullSubmeshesDS>)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<StaticMeshBatchElement>),	u_visibleBatchElements)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<Uint32>),					u_dispatchVisibleBatchElemsParams)
DS_END();


DS_BEGIN(SMCullMeshletsDS, rg::RGDescriptorSetState<SMCullMeshletsDS>)
	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<StaticMeshBatchElement>),	u_visibleBatchElements)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<SMGPUWorkloadID>),		u_meshletWorkloads)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<Uint32>),				u_dispatchMeshletsParams)
DS_END();


DS_BEGIN(SMCullTrianglesDS, rg::RGDescriptorSetState<SMCullTrianglesDS>)
	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<StaticMeshBatchElement>),		u_visibleBatchElements)
	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<SMGPUWorkloadID>),				u_meshletWorkloads)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<SMGPUWorkloadID>),			u_triangleWorkloads)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<SMIndirectDrawCallData>),	u_drawTrianglesParams)
DS_END();


DS_BEGIN(SMIndirectRenderTrianglesDS, rg::RGDescriptorSetState<SMIndirectRenderTrianglesDS>)
	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<StaticMeshBatchElement>),	u_visibleBatchElements)
	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<SMGPUWorkloadID>),			u_triangleWorkloads)
DS_END();


BEGIN_RG_NODE_PARAMETERS_STRUCT(SMIndirectTrianglesBatchDrawParams)
	RG_BUFFER_VIEW(batchDrawCommandsBuffer, rg::ERGBufferAccess::Read, rhi::EPipelineStage::DrawIndirect)
END_RG_NODE_PARAMETERS_STRUCT();


struct SMForwardOpaqueBatch
{
	lib::SharedPtr<rdr::Buffer> visibleBatchElementsBufferInstance;
	lib::SharedPtr<rdr::Buffer> dispatchVisibleBatchElemsParamsBufferInstance;

	rg::RGBufferViewHandle visibleBatchElementsDispatchParamsBuffer;
	rg::RGBufferViewHandle meshletsWorkloadsDispatchArgsBuffer;
	rg::RGBufferViewHandle drawTrianglesBatchArgsBuffer;
	
	lib::SharedPtr<StaticMeshBatchDS>			batchDS;
	
	lib::SharedPtr<SMCullSubmeshesDS>			cullSubmeshesDS;
	lib::SharedPtr<SMCullMeshletsDS>			cullMeshletsDS;
	lib::SharedPtr<SMCullTrianglesDS>			cullTrianglesDS;
	lib::SharedPtr<SMIndirectRenderTrianglesDS>	indirectRenderTrianglesDS;

	Uint32 batchedSubmeshesNum;
};


struct SMOpaqueForwardBatches
{
	lib::DynamicArray<SMForwardOpaqueBatch> batches;
};

//////////////////////////////////////////////////////////////////////////////////////////////////
// DepthPrepass ==================================================================================

BEGIN_SHADER_STRUCT(SMDepthPrepassDrawCallData)
	/* Vulkan command data */
	SHADER_STRUCT_FIELD(Uint32, vertexCount)
	SHADER_STRUCT_FIELD(Uint32, instanceCount)
	SHADER_STRUCT_FIELD(Uint32, firstVertex)
	SHADER_STRUCT_FIELD(Uint32, firstInstance)
	
	/* Custom Data */
	SHADER_STRUCT_FIELD(Uint32, batchElementIdx)
	SHADER_STRUCT_FIELD(Uint32, padding0)
	SHADER_STRUCT_FIELD(Uint32, padding1)
	SHADER_STRUCT_FIELD(Uint32, padding2)
END_SHADER_STRUCT();


DS_BEGIN(SMDepthPrepassCullInstancesDS, rg::RGDescriptorSetState<SMDepthPrepassCullInstancesDS>)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<SMDepthPrepassDrawCallData>),	u_drawCommands)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<Uint32>),						u_drawsCount)
	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<Uint32>),							u_validBatchElementsNum)
DS_END();


DS_BEGIN(SMDepthPrepassDrawInstancesDS, rg::RGDescriptorSetState<SMDepthPrepassDrawInstancesDS>)
	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<SMDepthPrepassDrawCallData>),	u_drawCommands)
DS_END();


BEGIN_RG_NODE_PARAMETERS_STRUCT(SMIndirectDepthPrepassCommandsParameters)
	RG_BUFFER_VIEW(batchDrawCommandsBuffer, rg::ERGBufferAccess::Read, rhi::EPipelineStage::DrawIndirect)
	RG_BUFFER_VIEW(batchDrawCommandsCountBuffer, rg::ERGBufferAccess::Read, rhi::EPipelineStage::DrawIndirect)
END_RG_NODE_PARAMETERS_STRUCT();


struct SMDepthPrepassBatch
{
	lib::SharedPtr<StaticMeshBatchDS>				batchDS;
	
	lib::SharedPtr<SMDepthPrepassCullInstancesDS>	cullInstancesDS;
	lib::SharedPtr<SMDepthPrepassDrawInstancesDS>	drawInstancesDS;

	rg::RGBufferViewHandle drawCommandsBuffer;
	rg::RGBufferViewHandle indirectDrawCountBuffer;

	Uint32 batchedSubmeshesNum;
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

SMDepthPrepassBatch CreateDepthPrepassBatch(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, const StaticMeshesVisibleLastFrame::BatchElementsInfo& batchElements)
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

	const rhi::BufferDefinition commandsBufferDef(sizeof(SMDepthPrepassDrawCallData) * batchElements.batchedSubmeshesNum, lib::Flags(rhi::EBufferUsage::Storage, rhi::EBufferUsage::Indirect));
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

	const lib::SharedRef<SMDepthPrepassDrawInstancesDS> drawInstancesDS = rdr::ResourcesManager::CreateDescriptorSetState<SMDepthPrepassDrawInstancesDS>(RENDERER_RESOURCE_NAME("SMDepthPrepassDrawInstancesDS"));
	drawInstancesDS->u_drawCommands = drawCommandsBuffer;

	batch.cullInstancesDS = cullInstancesDS;
	batch.drawInstancesDS = drawInstancesDS;

	batch.drawCommandsBuffer = drawCommandsBuffer;
	batch.indirectDrawCountBuffer = indirectDrawCountBuffer;

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
		compilationSettings.AddShaderToCompile(sc::ShaderStageCompilationDef(rhi::EShaderStage::Vertex, "StaticMesh_DepthPrepassVS"));
		compilationSettings.AddShaderToCompile(sc::ShaderStageCompilationDef(rhi::EShaderStage::Fragment, "StaticMesh_DepthPrepassFS"));
		const rdr::ShaderID depthPrepassShader = rdr::ResourcesManager::CreateShader("Sculptor/StaticMeshes/StaticMesh_RenderDepthPrepass.hlsl", compilationSettings);

		rhi::GraphicsPipelineDefinition pipelineDef;
		pipelineDef.primitiveTopology = rhi::EPrimitiveTopology::TriangleList;
		pipelineDef.renderTargetsDefinition.depthRTDefinition.format = DepthPrepassRenderStage::GetDepthFormat();
		m_depthPrepassRenderingPipeline = rdr::ResourcesManager::CreateGfxPipeline(RENDERER_RESOURCE_NAME("StaticMesh_DepthPrepassPipeline"), pipelineDef, depthPrepassShader);
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

			RenderStageEntries& depthPrepassStageEntries = viewSpec.GetRenderStageEntries(ERenderStage::DepthPrepass);
			depthPrepassStageEntries.GetOnRenderStage().AddRawMember(this, &StaticMeshesRenderSystem::RenderDepthPrepassPerView);
		}
	}

	if (viewSpec.SupportsStage(ERenderStage::ForwardOpaque))
	{
		const Bool hasAnyBatches = BuildForwardOpaueBatchesPerView(graphBuilder, renderScene, viewSpec);

		if (hasAnyBatches)
		{
			RenderStageEntries& depthPrepassStageEntries = viewSpec.GetRenderStageEntries(ERenderStage::DepthPrepass);
			depthPrepassStageEntries.GetPostRenderStage().AddRawMember(this, &StaticMeshesRenderSystem::CullForwardOpaquePerView);

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

		for (const StaticMeshesVisibleLastFrame::BatchElementsInfo& instances : visibleInstances->visibleInstances)
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

void StaticMeshesRenderSystem::RenderDepthPrepassPerView(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& context)
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
									recorder.DrawIndirectCount(drawsBufferView, 0, sizeof(SMDepthPrepassDrawCallData), drawCountBufferView, 0, maxDrawCallsNum);
								});
	}
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

void StaticMeshesRenderSystem::CullForwardOpaquePerView(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& context)
{
	SPT_PROFILER_FUNCTION();

	const DepthPrepassData& prepassData = viewSpec.GetData().Get<DepthPrepassData>();
	SPT_CHECK(prepassData.hiZ.IsValid());

	SMDepthCullingParams depthCullingParams;
	depthCullingParams.hiZResolution = prepassData.hiZ->GetResolution().head<2>().cast<Real32>();

	const lib::SharedRef<SMDepthCullingDS> depthCullingDS = rdr::ResourcesManager::CreateDescriptorSetState<SMDepthCullingDS>(RENDERER_RESOURCE_NAME("SMDepthCullingDS"));
	depthCullingDS->u_hiZTexture			= prepassData.hiZ;
	depthCullingDS->u_depthCullingParams	= depthCullingParams;

	const SMOpaqueForwardBatches& forwardOpaqueBatches = viewSpec.GetData().Get<SMOpaqueForwardBatches>();

	const SMRenderingViewData& staticMeshRenderingViewData = viewSpec.GetData().Get<SMRenderingViewData>();

	const rg::BindDescriptorSetsScope staticMeshCullingDSScope(graphBuilder,
															   rg::BindDescriptorSets(lib::Ref(StaticMeshUnifiedData::Get().GetUnifiedDataDS()),
																					  lib::Ref(staticMeshRenderingViewData.viewDS)));

	for (const SMForwardOpaqueBatch& batch : forwardOpaqueBatches.batches)
	{
		const rg::BindDescriptorSetsScope staticMeshBatchDSScope(graphBuilder, rg::BindDescriptorSets(lib::Ref(batch.batchDS)));

		const Uint32 dispatchGroupsNum = math::Utils::RoundUp<Uint32>(batch.batchedSubmeshesNum, 64) / 64;

		graphBuilder.Dispatch(RG_DEBUG_NAME("SM Cull Submeshes"),
									  m_cullSubmeshesPipeline,
									  math::Vector3u(dispatchGroupsNum, 1, 1),
									  rg::BindDescriptorSets(lib::Ref(batch.cullSubmeshesDS), depthCullingDS));
	}

	for (const SMForwardOpaqueBatch& batch : forwardOpaqueBatches.batches)
	{
		const rg::BindDescriptorSetsScope staticMeshBatchDSScope(graphBuilder, rg::BindDescriptorSets(lib::Ref(batch.batchDS)));

		graphBuilder.DispatchIndirect(RG_DEBUG_NAME("SM Cull Meshlets"),
									  m_cullMeshletsPipeline,
									  batch.visibleBatchElementsDispatchParamsBuffer,
									  0,
									  rg::BindDescriptorSets(lib::Ref(batch.cullMeshletsDS), depthCullingDS));
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

} // spt::rsc
