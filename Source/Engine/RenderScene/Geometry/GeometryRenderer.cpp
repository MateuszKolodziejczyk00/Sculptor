#include "GeometryRenderer.h"
#include "ResourcesManager.h"
#include "RenderGraphBuilder.h"
#include "RGDescriptorSetState.h"
#include "DescriptorSetBindings/RWBufferBinding.h"
#include "DescriptorSetBindings/ConstantBufferBinding.h"
#include "DescriptorSetBindings/SRVTextureBinding.h"
#include "StaticMeshes/StaticMeshGeometry.h"
#include "Materials/MaterialsUnifiedData.h"
#include "Common/ShaderCompilationInput.h"
#include "View/ViewRenderingSpec.h"
#include "GeometryManager.h"
#include "SceneRenderer/RenderStages/Utils/hiZRenderer.h"


namespace spt::rsc
{

namespace geometry_rendering
{

enum class EGeometryVisPass
{
	// 1st pass: Renders geometry that was visible in the previous frame
	First,
	// 2nd pass: Renders geometry that was disoccluded in the current frame
	Second,
	Num
};


struct VisibleMeshletsOutput
{
	rg::RGBufferViewHandle visibleMeshlets;
	rg::RGBufferViewHandle visibleMeshletsCount;
};


struct GeometryPassParams
{
	EGeometryVisPass      passIdx = EGeometryVisPass::Num;
	VisibleMeshletsOutput visibleMeshletsOutput;
};

namespace priv
{

struct BatchDrawCommands
{
	rg::RGBufferViewHandle drawCommands;
	rg::RGBufferViewHandle drawCommandsCount;
};


BEGIN_SHADER_STRUCT(GeometryDrawMeshTaskCommand)
	SHADER_STRUCT_FIELD(Uint32, dispatchGroupsX)
	SHADER_STRUCT_FIELD(Uint32, dispatchGroupsY)
	SHADER_STRUCT_FIELD(Uint32, dispatchGroupsZ)
	SHADER_STRUCT_FIELD(Uint32, batchElemIdx)
END_SHADER_STRUCT();


DS_BEGIN(GeometryCullSubmeshesDS, rg::RGDescriptorSetState<GeometryCullSubmeshesDS>)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<GeometryDrawMeshTaskCommand>), u_drawCommands)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<Uint32>),                      u_drawCommandsCount)
DS_END();


BEGIN_SHADER_STRUCT(VisCullingParams)
	SHADER_STRUCT_FIELD(math::Vector2f, hiZResolution)
	SHADER_STRUCT_FIELD(math::Vector2f, historyHiZResolution)
	SHADER_STRUCT_FIELD(Bool,           hasHistoryHiZ)
	SHADER_STRUCT_FIELD(Uint32,         passIdx)
END_SHADER_STRUCT();


DS_BEGIN(VisCullingDS, rg::RGDescriptorSetState<VisCullingDS>)
	DS_BINDING(BINDING_TYPE(gfx::OptionalSRVTexture2DBinding<Real32>),                              u_hiZTexture) // valid only for 2nd pass
	DS_BINDING(BINDING_TYPE(gfx::OptionalSRVTexture2DBinding<Real32>),                              u_historyHiZTexture)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::LinearMinClampToEdge>), u_hiZSampler)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<VisCullingParams>),                          u_visCullingParams)
DS_END();


BEGIN_RG_NODE_PARAMETERS_STRUCT(IndirectGeometryBatchDrawParams)
	RG_BUFFER_VIEW(drawCommands, rg::ERGBufferAccess::Read, rhi::EPipelineStage::DrawIndirect)
	RG_BUFFER_VIEW(drawCommandsCount, rg::ERGBufferAccess::Read, rhi::EPipelineStage::DrawIndirect)
END_RG_NODE_PARAMETERS_STRUCT();


DS_BEGIN(GeometryDrawMeshesDS, rg::RGDescriptorSetState<GeometryDrawMeshesDS>)
	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<GeometryDrawMeshTaskCommand>), u_drawCommands)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<GPUVisibleMeshlet>),         u_visibleMeshlets)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<Uint32>),                    u_visibleMeshletsCount)
DS_END();


static lib::MTHandle<VisCullingDS> CreateCullingDS(const VisPassParams& visPassParams, EGeometryVisPass passIdx)
{
	SPT_CHECK(passIdx < EGeometryVisPass::Num);

	const Bool hasHistoryHiZ = visPassParams.historyHiZ.IsValid();

	VisCullingParams visCullingParams;
	visCullingParams.hiZResolution        = visPassParams.hiZ->GetResolution2D().cast<Real32>();
	visCullingParams.historyHiZResolution = hasHistoryHiZ ? visPassParams.historyHiZ->GetResolution2D().cast<Real32>() : math::Vector2f{};
	visCullingParams.hasHistoryHiZ        = hasHistoryHiZ;
	visCullingParams.passIdx              = static_cast<Uint32>(passIdx);

	lib::MTHandle<VisCullingDS> visCullingDS = rdr::ResourcesManager::CreateDescriptorSetState<VisCullingDS>(RENDERER_RESOURCE_NAME("VisCullingDS"));
	visCullingDS->u_hiZTexture        = visPassParams.hiZ;
	visCullingDS->u_historyHiZTexture = visPassParams.historyHiZ;
	visCullingDS->u_visCullingParams  = visCullingParams;

	return visCullingDS;
}


static rdr::PipelineStateID CreatePipelineForBatch(const VisPassParams& visPassParams, const GeometryPassParams& geometryPass, const GeometryBatch& batch)
{
	SPT_PROFILER_FUNCTION();

	sc::ShaderCompilationSettings compilationSettings;
	if (geometryPass.passIdx == EGeometryVisPass::First)
	{
		compilationSettings.AddMacroDefinition(sc::MacroDefinition("GEOMETRY_PASS_IDX", "1"));
	}
	else
	{
		compilationSettings.AddMacroDefinition(sc::MacroDefinition("GEOMETRY_PASS_IDX", "2"));
	}

	const rdr::ShaderID taskShader = rdr::ResourcesManager::CreateShader("Sculptor/GeometryRendering/GeometryVisibilityBuffer.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Task, "GeometryVisibilityTS"), compilationSettings);
	const rdr::ShaderID meshShader = rdr::ResourcesManager::CreateShader("Sculptor/GeometryRendering/GeometryVisibilityBuffer.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Mesh, "GeometryVisibilityMS"), compilationSettings);
	const rdr::ShaderID fragmentShader = rdr::ResourcesManager::CreateShader("Sculptor/GeometryRendering/GeometryVisibilityBuffer.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Fragment, "GeometryVisibilityFS"), compilationSettings);

	rdr::GraphicsPipelineShaders shaders;
	shaders.taskShader = taskShader;
	shaders.meshShader = meshShader;
	shaders.fragmentShader = fragmentShader;

	rhi::GraphicsPipelineDefinition pipelineDef;
	pipelineDef.primitiveTopology = rhi::EPrimitiveTopology::TriangleList;
	pipelineDef.renderTargetsDefinition.depthRTDefinition = rhi::DepthRenderTargetDefinition(rhi::EFragmentFormat::D32_S_Float, rhi::ECompareOp::Greater);
	pipelineDef.renderTargetsDefinition.colorRTsDefinition.emplace_back(rhi::ColorRenderTargetDefinition(rhi::EFragmentFormat::R32_U_Int, rhi::ERenderTargetBlendType::Disabled));
	
	const rdr::PipelineStateID pipeline = rdr::ResourcesManager::CreateGfxPipeline(RENDERER_RESOURCE_NAME("Geometry Visibility Pipeline"), shaders, pipelineDef);

	return pipeline;
}


static lib::DynamicArray<BatchDrawCommands> BuildBatchDrawCommands(rg::RenderGraphBuilder& graphBuilder, const VisPassParams& visPassParams)
{
	SPT_PROFILER_FUNCTION();

	const rdr::ShaderID cullSubmeshesShader = rdr::ResourcesManager::CreateShader("Sculptor/GeometryRendering/Geometry_CullSubmeshes.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "CullSubmeshesCS"));
	static const rdr::PipelineStateID cullSubmeshesPipeline = rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("Geometry_CullSubmeshesPipeline"), cullSubmeshesShader);
	
	lib::DynamicArray<BatchDrawCommands> batchDrawCommands;
	batchDrawCommands.reserve(visPassParams.geometryPassData.geometryBatches.size());

	for (const GeometryBatch& batch : visPassParams.geometryPassData.geometryBatches)
	{
		rhi::BufferDefinition drawCommandsBufferDef;
		drawCommandsBufferDef.size  = sizeof(GeometryDrawMeshTaskCommand) * batch.batchElementsNum;
		drawCommandsBufferDef.usage = lib::Flags(rhi::EBufferUsage::Storage, rhi::EBufferUsage::Indirect);
		const rg::RGBufferViewHandle drawCommands = graphBuilder.CreateBufferView(RG_DEBUG_NAME("Draw Mesh Commands"), drawCommandsBufferDef, rhi::EMemoryUsage::GPUOnly);

		rhi::BufferDefinition drawCommandsCountBufferDef;
		drawCommandsCountBufferDef.size  = sizeof(Uint32);
		drawCommandsCountBufferDef.usage = lib::Flags(rhi::EBufferUsage::Storage, rhi::EBufferUsage::Indirect, rhi::EBufferUsage::TransferDst);
		const rg::RGBufferViewHandle drawCommandsCount = graphBuilder.CreateBufferView(RG_DEBUG_NAME("Draw Mesh Commands Count"), drawCommandsCountBufferDef, rhi::EMemoryUsage::GPUOnly);

		graphBuilder.FillBuffer(RG_DEBUG_NAME("Initialize Draw Commands Count"), drawCommandsCount, 0u, drawCommandsCount->GetSize(), 0u);

		const lib::MTHandle<GeometryCullSubmeshesDS> cullSubmeshesDS = rdr::ResourcesManager::CreateDescriptorSetState<GeometryCullSubmeshesDS>(RENDERER_RESOURCE_NAME("GeometryCullSubmeshesDS"));
		cullSubmeshesDS->u_drawCommands         = drawCommands;
		cullSubmeshesDS->u_drawCommandsCount    = drawCommandsCount;

		const Uint32 groupSize = 64u;
		const Uint32 dispatchGroups = math::Utils::DivideCeil(batch.batchElementsNum, groupSize);

		graphBuilder.Dispatch(RG_DEBUG_NAME("Cull Submeshes"),
							  cullSubmeshesPipeline,
							  dispatchGroups,
							  rg::BindDescriptorSets(batch.batchDS, cullSubmeshesDS));

		batchDrawCommands.emplace_back(BatchDrawCommands{ drawCommands, drawCommandsCount });
	}

	return batchDrawCommands;
}


static void CreateRenderPass(rg::RenderGraphBuilder& graphBuilder, const VisPassParams& visPassParams, const GeometryPassParams& geometryPass)
{
	SPT_PROFILER_FUNCTION();

	const RenderView& renderView = visPassParams.viewSpec.GetRenderView();

	const math::Vector2u resolution = renderView.GetRenderingRes();

	rg::RGRenderPassDefinition renderPassDef(math::Vector2i(0, 0), resolution);

	const rhi::ERTLoadOperation renderTargetLoadOp = geometryPass.passIdx == EGeometryVisPass::First ? rhi::ERTLoadOperation::Clear : rhi::ERTLoadOperation::Load;

	rg::RGRenderTargetDef depthRTDef;
	depthRTDef.textureView    = visPassParams.depth;
	depthRTDef.loadOperation  = renderTargetLoadOp;
	depthRTDef.storeOperation = rhi::ERTStoreOperation::Store;
	depthRTDef.clearColor     = rhi::ClearColor(0.f);
	renderPassDef.SetDepthRenderTarget(depthRTDef);

	if (visPassParams.visibilityTexture.IsValid())
	{
		rg::RGRenderTargetDef visibilityRTDef;
		visibilityRTDef.textureView = visPassParams.visibilityTexture;
		visibilityRTDef.loadOperation = renderTargetLoadOp;
		visibilityRTDef.storeOperation = rhi::ERTStoreOperation::Store;
		visibilityRTDef.clearColor = rhi::ClearColor(0, 0, 0, 0);
		renderPassDef.AddColorRenderTarget(visibilityRTDef);
	}
	
	graphBuilder.RenderPass(RG_DEBUG_NAME("Visibility Pass"),
							renderPassDef,
							rg::EmptyDescriptorSets(),
							[resolution](const lib::SharedRef<rdr::RenderContext>& renderContext, rdr::CommandRecorder& recorder)
							{
								recorder.SetViewport(math::AlignedBox2f(math::Vector2f(0.f, 0.f), resolution.cast<Real32>()), 0.f, 1.f);
								recorder.SetScissor(math::AlignedBox2u(math::Vector2u(0, 0), resolution));
							});
}


static void ExecuteBatchDrawCommands(rg::RenderGraphBuilder& graphBuilder, const VisPassParams& visPassParams, const GeometryPassParams& geometryPass, lib::Span<const BatchDrawCommands> commands)
{
	SPT_PROFILER_FUNCTION();

	CreateRenderPass(graphBuilder, visPassParams, geometryPass);

	for (SizeType batchIdx = 0; batchIdx < visPassParams.geometryPassData.geometryBatches.size(); ++batchIdx)
	{
		const GeometryBatch& batch            = visPassParams.geometryPassData.geometryBatches[batchIdx];
		const BatchDrawCommands& drawCommands = commands[batchIdx];

		IndirectGeometryBatchDrawParams indirectDrawParams;
		indirectDrawParams.drawCommands      = drawCommands.drawCommands;
		indirectDrawParams.drawCommandsCount = drawCommands.drawCommandsCount;

		lib::MTHandle<GeometryDrawMeshesDS> drawMeshesDS = rdr::ResourcesManager::CreateDescriptorSetState<GeometryDrawMeshesDS>(RENDERER_RESOURCE_NAME("GeometryDrawMeshesDS"));
		drawMeshesDS->u_drawCommands         = drawCommands.drawCommands;
		drawMeshesDS->u_visibleMeshlets      = geometryPass.visibleMeshletsOutput.visibleMeshlets;
		drawMeshesDS->u_visibleMeshletsCount = geometryPass.visibleMeshletsOutput.visibleMeshletsCount;

		const rdr::PipelineStateID pipeline = CreatePipelineForBatch(visPassParams, geometryPass, batch);

		const Uint32 maxDrawsCount = batch.batchElementsNum;

		graphBuilder.AddSubpass(RG_DEBUG_NAME("Batch Subpass"),
								rg::BindDescriptorSets(drawMeshesDS, batch.batchDS, GeometryManager::Get().GetGeometryDSState()),
								std::tie(indirectDrawParams),
								[indirectDrawParams, pipeline, maxDrawsCount]
								(const lib::SharedRef<rdr::RenderContext>& renderContext, rdr::CommandRecorder& recorder)
								{
									recorder.BindGraphicsPipeline(pipeline);

									const rdr::BufferView& drawCommandsView = indirectDrawParams.drawCommands->GetResource();
									const rdr::BufferView& drawCommandsCountView = indirectDrawParams.drawCommandsCount->GetResource();

									recorder.DrawMeshTasksIndirectCount(drawCommandsView.GetBuffer(),
																		drawCommandsView.GetOffset(),
																		sizeof(GeometryDrawMeshTaskCommand),
																		drawCommandsCountView.GetBuffer(),
																		drawCommandsCountView.GetOffset(),
																		maxDrawsCount);
								});
	}
}

} // priv

static void RenderGeometryPrologue(rg::RenderGraphBuilder& graphBuilder, const VisPassParams& visPassParams, const VisibleMeshletsOutput& visibleMeshletsOutput)
{
	SPT_PROFILER_FUNCTION();

	graphBuilder.FillBuffer(RG_DEBUG_NAME("Reset Visible Meshlets Count"), visibleMeshletsOutput.visibleMeshletsCount, 0u, visibleMeshletsOutput.visibleMeshletsCount->GetSize(), 0u);
}


static void RenderGeometryVisPass(rg::RenderGraphBuilder& graphBuilder, const VisPassParams& visPassParams, const GeometryPassParams& geometryPass)
{
	SPT_PROFILER_FUNCTION();

	const ViewRenderingSpec& viewSpec = visPassParams.viewSpec;
	const RenderView& renderView      = viewSpec.GetRenderView();

	const lib::MTHandle<priv::VisCullingDS> cullingDS = priv::CreateCullingDS(visPassParams, geometryPass.passIdx);

	const rg::BindDescriptorSetsScope geometryCullingDSScope(graphBuilder,
															 rg::BindDescriptorSets(StaticMeshUnifiedData::Get().GetUnifiedDataDS(),
																					renderView.GetRenderViewDS(),
																					cullingDS));

	const lib::DynamicArray<priv::BatchDrawCommands> batchDrawCommands = priv::BuildBatchDrawCommands(graphBuilder, visPassParams);

	priv::ExecuteBatchDrawCommands(graphBuilder, visPassParams, geometryPass, { batchDrawCommands });

	HiZ::CreateHierarchicalZ(graphBuilder, visPassParams.depth, visPassParams.hiZ->GetTexture());
}

} // geometry_rendering


GeometryRenderer::GeometryRenderer()
{
	rhi::BufferDefinition countBufferDef;
	countBufferDef.size  = sizeof(Uint32);
	countBufferDef.usage = lib::Flags(rhi::EBufferUsage::Storage, rhi::EBufferUsage::TransferDst);
	m_visibleMeshletsCount = rdr::ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME("Visible Meshlets Count"), countBufferDef, rhi::EMemoryUsage::GPUOnly);

	// TODO handle resizing, use smaller default size
	const Uint64 defaultSize = sizeof(GPUVisibleMeshlet) * 1024 * 128;
	
	rhi::BufferDefinition meshletsBufferDef;
	meshletsBufferDef.size  = defaultSize;
	meshletsBufferDef.usage = lib::Flags(rhi::EBufferUsage::Storage, rhi::EBufferUsage::TransferDst);
	m_visibleMeshlets = rdr::ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME("Visible Meshlets"), meshletsBufferDef, rhi::EMemoryUsage::GPUOnly);
}

void GeometryRenderer::RenderVisibility(rg::RenderGraphBuilder& graphBuilder, const VisPassParams& visPassParams)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(visPassParams.depth.IsValid());
	SPT_CHECK(visPassParams.hiZ.IsValid());
	SPT_CHECK(visPassParams.visibilityTexture.IsValid());

	geometry_rendering::VisibleMeshletsOutput visibleMeshletsOutput;
	visibleMeshletsOutput.visibleMeshlets      = graphBuilder.AcquireExternalBufferView(m_visibleMeshletsCount->CreateFullView());;
	visibleMeshletsOutput.visibleMeshletsCount = graphBuilder.AcquireExternalBufferView(m_visibleMeshlets->CreateFullView());;

	geometry_rendering::RenderGeometryPrologue(graphBuilder, visPassParams, visibleMeshletsOutput);

	// 1st pass: Renders geometry that was visible in the previous frame
	geometry_rendering::GeometryPassParams firstPassParams;
	firstPassParams.passIdx = geometry_rendering::EGeometryVisPass::First;
	firstPassParams.visibleMeshletsOutput = visibleMeshletsOutput;

	geometry_rendering::RenderGeometryVisPass(graphBuilder, visPassParams, firstPassParams);

	// 2nd pass: Renders geometry that was disoccluded in the current frame

	geometry_rendering::GeometryPassParams secondPassParams;
	secondPassParams.passIdx = geometry_rendering::EGeometryVisPass::Second;
	secondPassParams.visibleMeshletsOutput = visibleMeshletsOutput;

	geometry_rendering::RenderGeometryVisPass(graphBuilder, visPassParams, secondPassParams);
}

} // spt::rsc
