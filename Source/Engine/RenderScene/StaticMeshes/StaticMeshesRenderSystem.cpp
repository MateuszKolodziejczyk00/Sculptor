#include "StaticMeshesRenderSystem.h"
#include "StaticMeshPrimitivesSystem.h"
#include "ShaderStructs/ShaderStructsMacros.h"
#include "RenderGraphBuilder.h"
#include "View/ViewRenderingSpec.h"
#include "RenderScene.h"
#include "DescriptorSetBindings/RWBufferBinding.h"
#include "RGDescriptorSetState.h"
#include "DescriptorSetBindings/ConstantBufferBinding.h"
#include "Types/DescriptorSetState/DescriptorSetState.h"
#include "Common/ShaderCompilationInput.h"
#include "GeometryManager.h"
#include "View/RenderView.h"
#include "BufferUtilities.h"
#include "StaticMeshGeometry.h"

namespace spt::rsc
{

BEGIN_SHADER_STRUCT(, StaticMeshIndirectDrawCallData)
	SHADER_STRUCT_FIELD(Uint32, vertexCount)
	SHADER_STRUCT_FIELD(Uint32, instanceCount)
	SHADER_STRUCT_FIELD(Uint32, firstVertex)
	SHADER_STRUCT_FIELD(Uint32, firstInstance)
END_SHADER_STRUCT();


DS_BEGIN(, StaticMeshBatchDS, rg::RGDescriptorSetState<StaticMeshBatchDS>, DS_STAGES(rhi::EShaderStageFlags::Vertex, rhi::EShaderStageFlags::Fragment, rhi::EShaderStage::Compute))
	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<StaticMeshBatchElement>),	batchElements)
DS_END();


BEGIN_SHADER_STRUCT(, GPUWorkloadID)
	SHADER_STRUCT_FIELD(Uint32, data1)
	SHADER_STRUCT_FIELD(Uint32, data2)
END_SHADER_STRUCT();


DS_BEGIN(, StaticMeshSubmeshesWorkloadsDS, rg::RGDescriptorSetState<StaticMeshSubmeshesWorkloadsDS>, rhi::EShaderStageFlags::Compute)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<GPUWorkloadID>),		submeshesWorkloads)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<Uint32>),			indirectDispatchSubmeshesParams)
DS_END();


DS_BEGIN(, StaticMeshMeshletsWorkloadsDS, rg::RGDescriptorSetState<StaticMeshMeshletsWorkloadsDS>, rhi::EShaderStageFlags::Compute)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<GPUWorkloadID>),		meshletsWorkloads)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<Uint32>),			indirectDispatchMeshletsParams)
DS_END();


DS_BEGIN(, StaticMeshTrianglesWorkloadsDS, rg::RGDescriptorSetState<StaticMeshTrianglesWorkloadsDS>, rhi::EShaderStageFlags::Compute)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<GPUWorkloadID>),						trianglesWorkloads)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<StaticMeshIndirectDrawCallData>),	indirectDrawCommandParams)
DS_END();


DS_BEGIN(, StaticMeshTrianglesWorkloadsInputDS, rg::RGDescriptorSetState<StaticMeshTrianglesWorkloadsInputDS>, rhi::EShaderStageFlags::Vertex)
	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<GPUWorkloadID>),	trianglesWorkloads)
DS_END();


DS_BEGIN(, StaticMeshForwardOpaqueDS, rg::RGDescriptorSetState<StaticMeshForwardOpaqueDS>, rhi::EShaderStageFlags::Vertex)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<SceneViewData>),		sceneView)
	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<math::Matrix4f>),	instanceTransforms)
DS_END();


BEGIN_RG_NODE_PARAMETERS_STRUCT(StaticMeshIndirectBatchDrawParams)
	RG_BUFFER_VIEW(batchDrawCommandsBuffer, rg::ERGBufferAccess::Read, rhi::EPipelineStage::DrawIndirect)
END_RG_NODE_PARAMETERS_STRUCT();


struct StaticMeshBatchRenderingData
{
	lib::SharedPtr<StaticMeshBatchDS> batchDS;
	
	lib::SharedPtr<StaticMeshSubmeshesWorkloadsDS> submeshesWorkloadsDS;
	rg::RGBufferViewHandle submeshesWorkloadsDispatchArgsBuffer;
	
	lib::SharedPtr<StaticMeshMeshletsWorkloadsDS> meshletsWorkloadsDS;
	rg::RGBufferViewHandle meshletsWorkloadsDispatchArgsBuffer;
	
	lib::SharedPtr<StaticMeshTrianglesWorkloadsDS> trianglesWorkloadsDS;
	rg::RGBufferViewHandle trianglesWorkloadsDispatchArgsBuffer;

	lib::SharedPtr<StaticMeshTrianglesWorkloadsInputDS> trianglesWorkloadsInputDS;
};


struct StaticMeshBatchesViewData
{
	StaticMeshBatchRenderingData batch;
};

namespace utils
{

StaticMeshBatchRenderingData CreateBatchRenderingData(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, const StaticMeshBatch& batch)
{
	SPT_PROFILER_FUNCTION();
	
	StaticMeshBatchRenderingData batchRenderingData;

	const Uint64 batchDataSize = sizeof(StaticMeshBatchElement) * batch.batchElements.size();
	const rhi::BufferDefinition batchBufferDef(batchDataSize, rhi::EBufferUsage::Storage);
	const rhi::RHIAllocationInfo batchBufferAllocation(rhi::EMemoryUsage::CPUToGPU);
	const lib::SharedRef<rdr::Buffer> batchBuffer = rdr::ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME("StaticMeshesBatchBuffer"), batchBufferDef, batchBufferAllocation);

	gfx::UploadDataToBuffer(batchBuffer, 0, reinterpret_cast<const Byte*>(batch.batchElements.data()), batchDataSize);

	batchRenderingData.batchDS = rdr::ResourcesManager::CreateDescriptorSetState<StaticMeshBatchDS>(RENDERER_RESOURCE_NAME("StaticMeshesBatchDS"));
	batchRenderingData.batchDS->batchElements = batchBuffer->CreateFullView();

	const rhi::EBufferUsage indirectBuffersUsageFlags = lib::Flags(rhi::EBufferUsage::Storage, rhi::EBufferUsage::Indirect, rhi::EBufferUsage::TransferDst);

	const rhi::RHIAllocationInfo workloadBuffersAllocation(rhi::EMemoryUsage::GPUOnly);
	
	// Create buffers for submeshes workloads
	const lib::SharedRef<StaticMeshSubmeshesWorkloadsDS> submeshesWorkloadsDS = rdr::ResourcesManager::CreateDescriptorSetState<StaticMeshSubmeshesWorkloadsDS>(RENDERER_RESOURCE_NAME("SMSubmeshesWorkloadsDS"));

	const rhi::BufferDefinition submeshesWorkloadsBufferDef(sizeof(GPUWorkloadID) * batch.maxSubmeshesNum, rhi::EBufferUsage::Storage);
	submeshesWorkloadsDS->submeshesWorkloads = graphBuilder.CreateBufferView(RG_DEBUG_NAME("SMSubmeshesWorkloadsBuffer"), submeshesWorkloadsBufferDef, workloadBuffersAllocation);

	const rhi::BufferDefinition indirectDispatchSubmeshesParamsBufferDef(sizeof(Uint32) * 3, indirectBuffersUsageFlags);
	const rg::RGBufferViewHandle submeshesWorkloadsDispatchArgsBuffer = graphBuilder.CreateBufferView(RG_DEBUG_NAME("IndirectDispatchSubmeshesParamsBuffer"), indirectDispatchSubmeshesParamsBufferDef, workloadBuffersAllocation);
	batchRenderingData.submeshesWorkloadsDispatchArgsBuffer = submeshesWorkloadsDispatchArgsBuffer;
	submeshesWorkloadsDS->indirectDispatchSubmeshesParams = submeshesWorkloadsDispatchArgsBuffer;
	graphBuilder.FillBuffer(RG_DEBUG_NAME("SetDefaultDispatchSubmeshesParams"), submeshesWorkloadsDispatchArgsBuffer, 0, sizeof(Uint32), 0);

	batchRenderingData.submeshesWorkloadsDS = submeshesWorkloadsDS;

	// Create buffers for meshlets workloads
	const lib::SharedRef<StaticMeshMeshletsWorkloadsDS> meshletsWorkloadsDS = rdr::ResourcesManager::CreateDescriptorSetState<StaticMeshMeshletsWorkloadsDS>(RENDERER_RESOURCE_NAME("SMMeshletsWorkloadsDS"));

	const rhi::BufferDefinition meshletsWorkloadsBufferDef(sizeof(GPUWorkloadID) * batch.maxMeshletsNum, rhi::EBufferUsage::Storage);
	meshletsWorkloadsDS->meshletsWorkloads = graphBuilder.CreateBufferView(RG_DEBUG_NAME("SMMeshletsWorkloadsBuffer"), meshletsWorkloadsBufferDef, workloadBuffersAllocation);

	const rhi::BufferDefinition indirectDispatchMeshletsParamsBufferDef(sizeof(Uint32) * 3, indirectBuffersUsageFlags);
	const rg::RGBufferViewHandle meshletsWorkloadsDispatchArgsBuffer = graphBuilder.CreateBufferView(RG_DEBUG_NAME("IndirectDispatchMeshletsParamsBuffer"), indirectDispatchMeshletsParamsBufferDef, workloadBuffersAllocation);
	batchRenderingData.meshletsWorkloadsDispatchArgsBuffer = meshletsWorkloadsDispatchArgsBuffer;
	meshletsWorkloadsDS->indirectDispatchMeshletsParams = meshletsWorkloadsDispatchArgsBuffer;
	graphBuilder.FillBuffer(RG_DEBUG_NAME("SetDefaultDispatchMeshletsParams"), meshletsWorkloadsDispatchArgsBuffer, 0, sizeof(Uint32), 0);

	batchRenderingData.meshletsWorkloadsDS = meshletsWorkloadsDS;

	// Create buffers for triangles workloads
	const lib::SharedRef<StaticMeshTrianglesWorkloadsDS> trianglesWorkloadsDS = rdr::ResourcesManager::CreateDescriptorSetState<StaticMeshTrianglesWorkloadsDS>(RENDERER_RESOURCE_NAME("SMTrianglesWorkloadsDS"));

	const rhi::BufferDefinition trianglesWorkloadsBufferDef(sizeof(GPUWorkloadID) * batch.maxTrianglesNum, rhi::EBufferUsage::Storage);
	const rg::RGBufferViewHandle trianglesWorkloadsBuffer = graphBuilder.CreateBufferView(RG_DEBUG_NAME("SMTrianglesWorkloadsBuffer"), trianglesWorkloadsBufferDef, workloadBuffersAllocation);
	trianglesWorkloadsDS->trianglesWorkloads = trianglesWorkloadsBuffer;

	const rhi::BufferDefinition indirectDispatchTrianglesParamsBufferDef(sizeof(StaticMeshIndirectDrawCallData), indirectBuffersUsageFlags);
	const rg::RGBufferViewHandle trianglesWorkloadsDispatchArgsBuffer = graphBuilder.CreateBufferView(RG_DEBUG_NAME("IndirectDispatchTrianglesParamsBuffer"), indirectDispatchTrianglesParamsBufferDef, workloadBuffersAllocation);
	batchRenderingData.trianglesWorkloadsDispatchArgsBuffer = trianglesWorkloadsDispatchArgsBuffer;
	trianglesWorkloadsDS->indirectDrawCommandParams = trianglesWorkloadsDispatchArgsBuffer;
	graphBuilder.FillBuffer(RG_DEBUG_NAME("SetDefaultDispatchTrianglesParams"), trianglesWorkloadsDispatchArgsBuffer, 0, sizeof(StaticMeshIndirectDrawCallData), 0);

	batchRenderingData.trianglesWorkloadsDS = trianglesWorkloadsDS;

	const lib::SharedRef<StaticMeshTrianglesWorkloadsInputDS> trianglesWorkloadsInputDS = rdr::ResourcesManager::CreateDescriptorSetState<StaticMeshTrianglesWorkloadsInputDS>(RENDERER_RESOURCE_NAME("SMTrianglesWorkloadsInputDS"));
	trianglesWorkloadsInputDS->trianglesWorkloads = trianglesWorkloadsBuffer;
	batchRenderingData.trianglesWorkloadsInputDS = trianglesWorkloadsInputDS;

	return batchRenderingData;
}

} // utils

StaticMeshesRenderSystem::StaticMeshesRenderSystem()
{
	m_supportedStages = lib::Flags(ERenderStage::GBufferGenerationStage, ERenderStage::ShadowGenerationStage);

	{
		sc::ShaderCompilationSettings compilationSettings;
		compilationSettings.AddShaderToCompile(sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "CullSubmeshesCS"));
		const rdr::ShaderID cullSubmeshesShader = rdr::ResourcesManager::CreateShader("StaticMeshes/StaticMesh_CullSubmeshes.hlsl", compilationSettings);
		cullSubmeshesPipeline = rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("StaticMesh_CullSubmeshesPipeline"), cullSubmeshesShader);
	}
	
	{
		sc::ShaderCompilationSettings compilationSettings;
		compilationSettings.AddShaderToCompile(sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "CullMeshletsCS"));
		const rdr::ShaderID cullSubmeshesShader = rdr::ResourcesManager::CreateShader("StaticMeshes/StaticMesh_CullMeshlets.hlsl", compilationSettings);
		cullMeshletsPipeline = rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("StaticMesh_CullMeshletsPipeline"), cullSubmeshesShader);
	}

	{
		sc::ShaderCompilationSettings compilationSettings;
		compilationSettings.AddShaderToCompile(sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "CullTrianglesCS"));
		const rdr::ShaderID cullSubmeshesShader = rdr::ResourcesManager::CreateShader("StaticMeshes/StaticMesh_CullTriangles.hlsl", compilationSettings);
		cullTrianglesPipeline = rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("StaticMesh_CullTrianglesPipeline"), cullSubmeshesShader);
	}

	{
		sc::ShaderCompilationSettings compilationSettings;
		compilationSettings.AddShaderToCompile(sc::ShaderStageCompilationDef(rhi::EShaderStage::Vertex, "StaticMeshVS"));
		compilationSettings.AddShaderToCompile(sc::ShaderStageCompilationDef(rhi::EShaderStage::Fragment, "StaticMeshFS"));
		const rdr::ShaderID generateGBufferShader = rdr::ResourcesManager::CreateShader("StaticMeshes/StaticMesh_ForwardOpaqueShading.hlsl", compilationSettings);

		rhi::GraphicsPipelineDefinition pipelineDef;
		pipelineDef.primitiveTopology = rhi::EPrimitiveTopology::TriangleList;
		pipelineDef.renderTargetsDefinition.depthRTDefinition.format = rhi::EFragmentFormat::D32_S_Float;
		pipelineDef.renderTargetsDefinition.colorRTsDefinition.emplace_back(rhi::ColorRenderTargetDefinition(rhi::EFragmentFormat::RGBA8_UN_Float));

		forwadOpaqueShadingPipeline = rdr::ResourcesManager::CreateGfxPipeline(RENDERER_RESOURCE_NAME("StaticMesh_ForwardOpaqueShading"), pipelineDef, generateGBufferShader);
	}
}

void StaticMeshesRenderSystem::RenderPerView(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec)
{
	SPT_PROFILER_FUNCTION();

	Super::RenderPerView(graphBuilder, renderScene, viewSpec);

	const StaticMeshPrimitivesSystem& staticMeshPrimsSystem = renderScene.GetPrimitivesSystemChecked<StaticMeshPrimitivesSystem>();

	const StaticMeshBatch batch = staticMeshPrimsSystem.BuildBatchForView(viewSpec.GetRenderView());

	if (batch.IsValid())
	{
		const StaticMeshBatchRenderingData batchRenderingData = utils::CreateBatchRenderingData(graphBuilder, renderScene, batch);
		const Uint32 batchElementsNum = static_cast<Uint32>(batch.batchElements.size());

		viewSpec.GetData().Create<StaticMeshBatchesViewData>(batchRenderingData);

		graphBuilder.Dispatch(RG_DEBUG_NAME("SM Cull Submeshes"),
							  cullSubmeshesPipeline,
							  math::Vector3u(batchElementsNum, 1, 1),
							  rg::BindDescriptorSets(lib::Ref(StaticMeshUnifiedData::Get().GetUnifiedDataDS()),
													 lib::Ref(batchRenderingData.batchDS),
													 lib::Ref(batchRenderingData.submeshesWorkloadsDS)));

		graphBuilder.DispatchIndirect(RG_DEBUG_NAME("SM Cull Meshlets"),
									  cullMeshletsPipeline,
									  batchRenderingData.submeshesWorkloadsDispatchArgsBuffer,
									  0,
									  rg::BindDescriptorSets(lib::Ref(StaticMeshUnifiedData::Get().GetUnifiedDataDS()),
															 lib::Ref(batchRenderingData.batchDS),
															 lib::Ref(batchRenderingData.submeshesWorkloadsDS),
															 lib::Ref(batchRenderingData.meshletsWorkloadsDS)));

		graphBuilder.DispatchIndirect(RG_DEBUG_NAME("SM Cull Triangles"),
									  cullTrianglesPipeline,
									  batchRenderingData.meshletsWorkloadsDispatchArgsBuffer,
									  0,
									  rg::BindDescriptorSets(lib::Ref(StaticMeshUnifiedData::Get().GetUnifiedDataDS()),
															 lib::Ref(batchRenderingData.batchDS),
															 lib::Ref(batchRenderingData.meshletsWorkloadsDS),
															 lib::Ref(batchRenderingData.trianglesWorkloadsDS),
															 lib::Ref(GeometryManager::Get().GetGeometryDSState())));

		if (viewSpec.SupportsStage(ERenderStage::GBufferGenerationStage))
		{
			RenderStageEntries& basePassStageEntries = viewSpec.GetRenderStageEntries(ERenderStage::GBufferGenerationStage);

			basePassStageEntries.GetOnRenderStage().AddRawMember(this, &StaticMeshesRenderSystem::RenderMeshesPerView);
		}
	}
}

void StaticMeshesRenderSystem::RenderMeshesPerView(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& context)
{
	SPT_PROFILER_FUNCTION();

	const StaticMeshBatchesViewData& viewBatches = viewSpec.GetData().Get<StaticMeshBatchesViewData>();

	const lib::SharedRef<StaticMeshForwardOpaqueDS> smDrawOpaqueDS = rdr::ResourcesManager::CreateDescriptorSetState<StaticMeshForwardOpaqueDS>(RENDERER_RESOURCE_NAME("StaticMeshForwardOpaqueDS"));
	smDrawOpaqueDS->instanceTransforms = renderScene.GetTransformsBuffer()->CreateFullView();
	smDrawOpaqueDS->sceneView = viewSpec.GetRenderView().GenerateViewData();

	const StaticMeshBatchRenderingData& batch = viewBatches.batch;

	StaticMeshIndirectBatchDrawParams drawParams;
	drawParams.batchDrawCommandsBuffer = batch.trianglesWorkloadsDispatchArgsBuffer;

	const lib::SharedRef<GeometryDS> unifiedGeometryDS = lib::Ref(GeometryManager::Get().GetGeometryDSState());

	graphBuilder.AddSubpass(RG_DEBUG_NAME("Render Static Meshes Batch"),
							rg::BindDescriptorSets(lib::Ref(StaticMeshUnifiedData::Get().GetUnifiedDataDS()),
												   lib::Ref(batch.batchDS),
												   smDrawOpaqueDS,
												   lib::Ref(batch.trianglesWorkloadsInputDS),
												   unifiedGeometryDS),
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

} // spt::rsc
