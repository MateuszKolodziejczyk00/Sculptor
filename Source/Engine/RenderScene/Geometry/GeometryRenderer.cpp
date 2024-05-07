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
#include "MaterialsSubsystem.h"
#include "MaterialsUnifiedData.h"


namespace spt::rsc
{

namespace geometry_rendering
{

enum class EGeometryVisPass
{
	// Renders geometry that was visible in the previous frame
	// Performs all culling operations + occlusion culling with last frame's HiZ
	// Outputs occluded batch elements that are in frustum for DisoccludedGeometryPass pass
	VisibleGeometryPass,
	// Renders batch elements that were disoccluded in the current frame
	// Performs occlusion culling with current frame's HiZ
	DisoccludedGeometryPass,
	// Renders meshlets that were disoccluded in the current frame
	// Note that this is different from DisoccludedGeometryPass pass, because it doesn't process whole batch elements
	DisoccludedMeshletsPass,
	Num
};


struct GeometryPassContext
{
	GeometryPassContext(rg::RGBufferViewHandle visibleMeshlets, rg::RGBufferViewHandle visibleMeshletsCount)
		: visibleMeshlets(visibleMeshlets)
		, visibleMeshletsCount(visibleMeshletsCount)
	{ }

	rg::RGBufferViewHandle visibleMeshlets;
	rg::RGBufferViewHandle visibleMeshletsCount;
};

namespace priv
{

BEGIN_SHADER_STRUCT(DispatchOccludedBatchElementsCommand)
	SHADER_STRUCT_FIELD(Uint32, dispatchGroupsX)
	SHADER_STRUCT_FIELD(Uint32, dispatchGroupsY)
	SHADER_STRUCT_FIELD(Uint32, dispatchGroupsZ)
END_SHADER_STRUCT();


BEGIN_SHADER_STRUCT(OccludedBatchElement)
	SHADER_STRUCT_FIELD(Uint32, batchElemIdx)
END_SHADER_STRUCT();


BEGIN_SHADER_STRUCT(OccludedMeshletData)
	SHADER_STRUCT_FIELD(Uint32, batchElemIdx)
	SHADER_STRUCT_FIELD(Uint32, localMeshletIdx)
END_SHADER_STRUCT();


BEGIN_SHADER_STRUCT(GeometryDrawMeshTaskCommand)
	SHADER_STRUCT_FIELD(Uint32, dispatchGroupsX)
	SHADER_STRUCT_FIELD(Uint32, dispatchGroupsY)
	SHADER_STRUCT_FIELD(Uint32, dispatchGroupsZ)
	SHADER_STRUCT_FIELD(Uint32, batchElemIdx)
END_SHADER_STRUCT();


DS_BEGIN(GeometryCullSubmeshes_VisibleGeometryPassDS, rg::RGDescriptorSetState<GeometryCullSubmeshes_VisibleGeometryPassDS>)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<GeometryDrawMeshTaskCommand>), u_drawCommands)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<Uint32>),                      u_drawCommandsCount)
	
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<OccludedBatchElement>),                 u_occludedBatchElements)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<Uint32>),                               u_occludedBatchElementsCount)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<DispatchOccludedBatchElementsCommand>), u_dispatchOccludedElementsCommand)
DS_END();


DS_BEGIN(GeometryCullSubmeshes_DisoccludedGeometryPassDS, rg::RGDescriptorSetState<GeometryCullSubmeshes_DisoccludedGeometryPassDS>)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<GeometryDrawMeshTaskCommand>), u_drawCommands)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<Uint32>),                      u_drawCommandsCount)

	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<OccludedBatchElement>), u_occludedBatchElements)
	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<Uint32>),               u_occludedBatchElementsCount)
DS_END();


BEGIN_SHADER_STRUCT(VisCullingParams)
	SHADER_STRUCT_FIELD(math::Vector2f, hiZResolution)
	SHADER_STRUCT_FIELD(math::Vector2f, historyHiZResolution)
	SHADER_STRUCT_FIELD(Bool,           hasHistoryHiZ)
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


DS_BEGIN(GeometryDrawMeshes_VisibleGeometryPassDS, rg::RGDescriptorSetState<GeometryDrawMeshes_VisibleGeometryPassDS>)
	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<GeometryDrawMeshTaskCommand>), u_drawCommands)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<GPUVisibleMeshlet>),         u_visibleMeshlets)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<Uint32>),                    u_visibleMeshletsCount)
	
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<OccludedMeshletData>),                  u_occludedMeshlets)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<Uint32>),                               u_occludedMeshletsCount)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<DispatchOccludedBatchElementsCommand>), u_occludedMeshletsDispatchCommand)
DS_END();


DS_BEGIN(GeometryDrawMeshes_DisoccludedGeometryPassDS, rg::RGDescriptorSetState<GeometryDrawMeshes_DisoccludedGeometryPassDS>)
	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<GeometryDrawMeshTaskCommand>), u_drawCommands)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<GPUVisibleMeshlet>),         u_visibleMeshlets)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<Uint32>),                    u_visibleMeshletsCount)
DS_END();


DS_BEGIN(GeometryDrawMeshes_DisoccludedMeshletsPassDS, rg::RGDescriptorSetState<GeometryDrawMeshes_DisoccludedMeshletsPassDS>)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<OccludedMeshletData>),       u_occludedMeshlets)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<Uint32>),                    u_occludedMeshletsCount)

	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<GPUVisibleMeshlet>),         u_visibleMeshlets)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<Uint32>),                    u_visibleMeshletsCount)
DS_END();


template<EGeometryVisPass passIdx>
struct GeometryPassTraits
{
};

template<>
struct GeometryPassTraits<EGeometryVisPass::VisibleGeometryPass>
{
	using DrawMeshesDSType = GeometryDrawMeshes_VisibleGeometryPassDS;

	static constexpr const char* GetPassName() { return "Visible Geometry Pass"; }
};

template<>
struct GeometryPassTraits<EGeometryVisPass::DisoccludedGeometryPass>
{
	using DrawMeshesDSType = GeometryDrawMeshes_DisoccludedGeometryPassDS;

	static constexpr const char* GetPassName() { return "Disoccluded Geometry Pass"; }
};

template<>
struct GeometryPassTraits<EGeometryVisPass::DisoccludedMeshletsPass>
{
	using DrawMeshesDSType = GeometryDrawMeshes_DisoccludedMeshletsPassDS;

	static constexpr const char* GetPassName() { return "Disoccluded Meshlets Pass"; }
};


struct BatchGPUData
{
	rg::RGBufferViewHandle drawCommands;
	rg::RGBufferViewHandle drawCommandsCount;
	
	rg::RGBufferViewHandle occludedBatchElements;
	rg::RGBufferViewHandle occludedBatchElementsCount;
	rg::RGBufferViewHandle dispatchOccludedBatchElementsCommand;

	rg::RGBufferViewHandle occludedMeshlets;
	rg::RGBufferViewHandle occludedMeshletsCount;
	rg::RGBufferViewHandle dispatchOccludedMeshletsCommand;
};


static BatchGPUData CreateGPUBatch(rg::RenderGraphBuilder& graphBuilder, const GeometryBatch& batch)
{
	SPT_PROFILER_FUNCTION();

	BatchGPUData batchGPUData;

	rhi::BufferDefinition drawCommandsBufferDef;
	drawCommandsBufferDef.size  = sizeof(GeometryDrawMeshTaskCommand) * batch.batchElementsNum;
	drawCommandsBufferDef.usage = lib::Flags(rhi::EBufferUsage::Storage, rhi::EBufferUsage::Indirect);
	const rg::RGBufferViewHandle drawCommands = graphBuilder.CreateBufferView(RG_DEBUG_NAME("Draw Mesh Commands"), drawCommandsBufferDef, rhi::EMemoryUsage::GPUOnly);

	rhi::BufferDefinition drawCommandsCountBufferDef;
	drawCommandsCountBufferDef.size  = sizeof(Uint32);
	drawCommandsCountBufferDef.usage = lib::Flags(rhi::EBufferUsage::Storage, rhi::EBufferUsage::Indirect, rhi::EBufferUsage::TransferDst);
	const rg::RGBufferViewHandle drawCommandsCount = graphBuilder.CreateBufferView(RG_DEBUG_NAME("Draw Mesh Commands Count"), drawCommandsCountBufferDef, rhi::EMemoryUsage::GPUOnly);

	rhi::BufferDefinition occludedBatchElements;
	occludedBatchElements.size  = sizeof(OccludedBatchElement) * batch.batchElementsNum;
	occludedBatchElements.usage = rhi::EBufferUsage::Storage;
	const rg::RGBufferViewHandle occludedBatchElementsView = graphBuilder.CreateBufferView(RG_DEBUG_NAME("Occluded Batch Elements"), occludedBatchElements, rhi::EMemoryUsage::GPUOnly);

	rhi::BufferDefinition occludedBatchElementsCount;
	occludedBatchElementsCount.size  = sizeof(Uint32);
	occludedBatchElementsCount.usage = lib::Flags(rhi::EBufferUsage::Storage, rhi::EBufferUsage::TransferDst);
	const rg::RGBufferViewHandle occludedBatchElementsCountView = graphBuilder.CreateBufferView(RG_DEBUG_NAME("Occluded Batch Elements Count"), occludedBatchElementsCount, rhi::EMemoryUsage::GPUOnly);

	rhi::BufferDefinition dispatchOccludedBatchElementsCommandDef;
	dispatchOccludedBatchElementsCommandDef.size  = sizeof(DispatchOccludedBatchElementsCommand);
	dispatchOccludedBatchElementsCommandDef.usage = lib::Flags(rhi::EBufferUsage::Storage, rhi::EBufferUsage::Indirect);
	const rg::RGBufferViewHandle dispatchOccludedBatchElementsCommand = graphBuilder.CreateBufferView(RG_DEBUG_NAME("Dispatch Occluded Batch Elements Command"), dispatchOccludedBatchElementsCommandDef, rhi::EMemoryUsage::GPUOnly);

	rhi::BufferDefinition occludedMeshlets;
	occludedMeshlets.size  = sizeof(OccludedMeshletData) * batch.batchMeshletsNum;
	occludedMeshlets.usage = rhi::EBufferUsage::Storage;
	const rg::RGBufferViewHandle occludedMeshletsView = graphBuilder.CreateBufferView(RG_DEBUG_NAME("Occluded Meshlets"), occludedMeshlets, rhi::EMemoryUsage::GPUOnly);

	rhi::BufferDefinition occludedMeshletsCount;
	occludedMeshletsCount.size  = sizeof(Uint32);
	occludedMeshletsCount.usage = lib::Flags(rhi::EBufferUsage::Storage, rhi::EBufferUsage::TransferDst);
	const rg::RGBufferViewHandle occludedMeshletsCountView = graphBuilder.CreateBufferView(RG_DEBUG_NAME("Occluded Meshlets Count"), occludedMeshletsCount, rhi::EMemoryUsage::GPUOnly);

	rhi::BufferDefinition dispatchOccludedMeshletsCommandDef;
	dispatchOccludedMeshletsCommandDef.size  = sizeof(DispatchOccludedBatchElementsCommand);
	dispatchOccludedMeshletsCommandDef.usage = lib::Flags(rhi::EBufferUsage::Storage, rhi::EBufferUsage::Indirect, rhi::EBufferUsage::TransferDst);
	const rg::RGBufferViewHandle dispatchOccludedMeshletsCommand = graphBuilder.CreateBufferView(RG_DEBUG_NAME("Dispatch Occluded Meshlets Command"), dispatchOccludedMeshletsCommandDef, rhi::EMemoryUsage::GPUOnly);

	batchGPUData.drawCommands                         = drawCommands;
	batchGPUData.drawCommandsCount                    = drawCommandsCount;
	batchGPUData.occludedBatchElements                = occludedBatchElementsView;
	batchGPUData.occludedBatchElementsCount           = occludedBatchElementsCountView;
	batchGPUData.dispatchOccludedBatchElementsCommand = dispatchOccludedBatchElementsCommand;
	batchGPUData.occludedMeshlets                     = occludedMeshletsView;
	batchGPUData.occludedMeshletsCount                = occludedMeshletsCountView;
	batchGPUData.dispatchOccludedMeshletsCommand      = dispatchOccludedMeshletsCommand;

	graphBuilder.FillFullBuffer(RG_DEBUG_NAME("Initialize Occluded Batch Elements Count"), batchGPUData.occludedBatchElementsCount, 0u);
	graphBuilder.FillFullBuffer(RG_DEBUG_NAME("Initialize Occluded Meshlets Count"), batchGPUData.occludedMeshletsCount, 0u);
	graphBuilder.FillFullBuffer(RG_DEBUG_NAME("Initialize Occluded Meshlets Command"), batchGPUData.dispatchOccludedMeshletsCommand, 0u);

	return batchGPUData;
}


static lib::MTHandle<VisCullingDS> CreateCullingDS(const VisPassParams& visPassParams)
{
	const Bool hasHistoryHiZ = visPassParams.historyHiZ.IsValid();

	VisCullingParams visCullingParams;
	visCullingParams.hiZResolution        = visPassParams.hiZ->GetResolution2D().cast<Real32>();
	visCullingParams.historyHiZResolution = hasHistoryHiZ ? visPassParams.historyHiZ->GetResolution2D().cast<Real32>() : math::Vector2f{};
	visCullingParams.hasHistoryHiZ        = hasHistoryHiZ;

	lib::MTHandle<VisCullingDS> visCullingDS = rdr::ResourcesManager::CreateDescriptorSetState<VisCullingDS>(RENDERER_RESOURCE_NAME("VisCullingDS"));
	visCullingDS->u_hiZTexture        = visPassParams.hiZ;
	visCullingDS->u_historyHiZTexture = visPassParams.historyHiZ;
	visCullingDS->u_visCullingParams  = visCullingParams;

	return visCullingDS;
}


template<EGeometryVisPass passIdx>
static sc::MacroDefinition GetPassIdxMacroDef()
{
	static constexpr const char* value[] = { "0", "1", "2" };
	return sc::MacroDefinition("GEOMETRY_PASS_IDX", value[static_cast<Uint32>(passIdx)]);
}


static mat::MaterialShadersHash GetMaterialShadersHash(const GeometryBatch& batch)
{
	if (batch.shader.IsDefaultShader())
	{
		switch (batch.shader.GetDefaultShader())
		{
		case GeometryBatchShader::EDefault::Opaque:
			return mat::MaterialShadersHash::NoMaterial();
		default:
			SPT_CHECK_MSG(false, "Unsupported default shader");
			return mat::MaterialShadersHash::NoMaterial();
		}
	}

	return batch.shader.GetCustomShader();
}


template<EGeometryVisPass passIdx>
static rdr::PipelineStateID CreatePipelineForBatch(const VisPassParams& visPassParams, const GeometryPassContext& passContext, const GeometryBatch& batch)
{
	SPT_PROFILER_FUNCTION();

	mat::MaterialShadersParameters shaderParams;
	shaderParams.macroDefinitions.emplace_back(GetPassIdxMacroDef<passIdx>());

	const mat::MaterialShadersHash materialShadersHash = GetMaterialShadersHash(batch);
	const mat::MaterialGraphicsShaders shaders = mat::MaterialsSubsystem::Get().GetMaterialShaders<mat::MaterialGraphicsShaders>("GeometryVisibility", materialShadersHash, shaderParams);

	rhi::GraphicsPipelineDefinition pipelineDef;
	pipelineDef.primitiveTopology = rhi::EPrimitiveTopology::TriangleList;
	pipelineDef.renderTargetsDefinition.depthRTDefinition = rhi::DepthRenderTargetDefinition(rhi::EFragmentFormat::D32_S_Float, rhi::ECompareOp::Greater);
	pipelineDef.renderTargetsDefinition.colorRTsDefinition.emplace_back(rhi::ColorRenderTargetDefinition(rhi::EFragmentFormat::R32_U_Int, rhi::ERenderTargetBlendType::Disabled));
	
	const rdr::PipelineStateID pipeline = rdr::ResourcesManager::CreateGfxPipeline(RENDERER_RESOURCE_NAME("Geometry Visibility Pipeline"),
																				   shaders.GetGraphicsPipelineShaders(),
																				   pipelineDef);

	return pipeline;
}


lib::DynamicArray<BatchGPUData> BuildGPUBatches(rg::RenderGraphBuilder& graphBuilder, const VisPassParams& visPassParams)
{
	SPT_PROFILER_FUNCTION();

	lib::DynamicArray<BatchGPUData> batchesGPUData;
	batchesGPUData.reserve(visPassParams.geometryPassData.geometryBatches.size());

	for (const GeometryBatch& batch : visPassParams.geometryPassData.geometryBatches)
	{
		BatchGPUData batchGPUData = CreateGPUBatch(graphBuilder, batch);
		batchesGPUData.emplace_back(std::move(batchGPUData));
	}

	return batchesGPUData;
}


template<EGeometryVisPass passIdx>
void CullBatchElements(rg::RenderGraphBuilder& graphBuilder, const VisPassParams& visPassParams, lib::Span<const BatchGPUData> gpuBatches)
{
	SPT_PROFILER_FUNCTION();

	static_assert(passIdx == EGeometryVisPass::VisibleGeometryPass || passIdx == EGeometryVisPass::DisoccludedGeometryPass);

	using CullSubmeshesDS = std::conditional_t<passIdx == EGeometryVisPass::VisibleGeometryPass, GeometryCullSubmeshes_VisibleGeometryPassDS, GeometryCullSubmeshes_DisoccludedGeometryPassDS>;

	sc::ShaderCompilationSettings compilationSettings;
	compilationSettings.AddMacroDefinition(GetPassIdxMacroDef<passIdx>());

	const rdr::ShaderID cullSubmeshesShader = rdr::ResourcesManager::CreateShader("Sculptor/GeometryRendering/Geometry_CullSubmeshes.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "CullSubmeshesCS"), compilationSettings);
	static const rdr::PipelineStateID cullSubmeshesPipeline = rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("Geometry_CullSubmeshesPipeline"), cullSubmeshesShader);
	
	for(SizeType batchIdx = 0u; batchIdx < visPassParams.geometryPassData.geometryBatches.size(); ++batchIdx)
	{
		const GeometryBatch& batch       = visPassParams.geometryPassData.geometryBatches[batchIdx];
		const BatchGPUData& batchGPUData = gpuBatches[batchIdx];

		graphBuilder.FillFullBuffer(RG_DEBUG_NAME("Initialize Draw Commands Count"), batchGPUData.drawCommandsCount, 0u);

		const lib::MTHandle<CullSubmeshesDS> cullSubmeshesDS = graphBuilder.CreateDescriptorSet<CullSubmeshesDS>(RENDERER_RESOURCE_NAME("CullSubmeshesDS"));
		cullSubmeshesDS->u_drawCommands               = batchGPUData.drawCommands;
		cullSubmeshesDS->u_drawCommandsCount          = batchGPUData.drawCommandsCount;
		cullSubmeshesDS->u_occludedBatchElements      = batchGPUData.occludedBatchElements;
		cullSubmeshesDS->u_occludedBatchElementsCount = batchGPUData.occludedBatchElementsCount;
		if constexpr (passIdx == EGeometryVisPass::VisibleGeometryPass)
		{
			cullSubmeshesDS->u_dispatchOccludedElementsCommand = batchGPUData.dispatchOccludedBatchElementsCommand;
		}

		if constexpr (passIdx == EGeometryVisPass::VisibleGeometryPass)
		{
			const Uint32 groupSize = 64u;
			const Uint32 dispatchGroups = math::Utils::DivideCeil(batch.batchElementsNum, groupSize);

			graphBuilder.Dispatch(RG_DEBUG_NAME_FORMATTED("Cull Submeshes ({})", GeometryPassTraits<passIdx>::GetPassName()),
								  cullSubmeshesPipeline,
								  dispatchGroups,
								  rg::BindDescriptorSets(batch.batchDS, cullSubmeshesDS));
		}
		else if constexpr (passIdx == EGeometryVisPass::DisoccludedGeometryPass)
		{
			graphBuilder.DispatchIndirect(RG_DEBUG_NAME_FORMATTED("Cull Submeshes ({})", GeometryPassTraits<passIdx>::GetPassName()),
										  cullSubmeshesPipeline,
										  batchGPUData.dispatchOccludedBatchElementsCommand,
										  0u,
										  rg::BindDescriptorSets(batch.batchDS, cullSubmeshesDS));
		}
	}
}


template<EGeometryVisPass passIdx>
static void CreateRenderPass(rg::RenderGraphBuilder& graphBuilder, const VisPassParams& visPassParams, const GeometryPassContext& passContext)
{
	SPT_PROFILER_FUNCTION();

	const RenderView& renderView = visPassParams.viewSpec.GetRenderView();

	const math::Vector2u resolution = renderView.GetRenderingRes();

	rg::RGRenderPassDefinition renderPassDef(math::Vector2i(0, 0), resolution);

	const rhi::ERTLoadOperation renderTargetLoadOp = passIdx == EGeometryVisPass::VisibleGeometryPass ? rhi::ERTLoadOperation::Clear : rhi::ERTLoadOperation::Load;

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
		visibilityRTDef.clearColor = rhi::ClearColor(idxNone<Uint32>, 0u, 0u, 0u);
		renderPassDef.AddColorRenderTarget(visibilityRTDef);
	}
	
	graphBuilder.RenderPass(RG_DEBUG_NAME_FORMATTED("Visibility Pass ({})", GeometryPassTraits<passIdx>::GetPassName()),
							renderPassDef,
							rg::EmptyDescriptorSets(),
							[resolution](const lib::SharedRef<rdr::RenderContext>& renderContext, rdr::CommandRecorder& recorder)
							{
								recorder.SetViewport(math::AlignedBox2f(math::Vector2f(0.f, 0.f), resolution.cast<Real32>()), 0.f, 1.f);
								recorder.SetScissor(math::AlignedBox2u(math::Vector2u(0, 0), resolution));
							});
}


template<EGeometryVisPass passIdx>
static void DrawBatchElements(rg::RenderGraphBuilder& graphBuilder, const VisPassParams& visPassParams, const GeometryPassContext& passContext, lib::Span<const BatchGPUData> gpuBatches)
{
	SPT_PROFILER_FUNCTION();

	static_assert(passIdx == EGeometryVisPass::VisibleGeometryPass || passIdx == EGeometryVisPass::DisoccludedGeometryPass);

	using GeometryDrawMeshesDS = typename GeometryPassTraits<passIdx>::DrawMeshesDSType;

	CreateRenderPass<passIdx>(graphBuilder, visPassParams, passContext);

	for (SizeType batchIdx = 0; batchIdx < visPassParams.geometryPassData.geometryBatches.size(); ++batchIdx)
	{
		const GeometryBatch& batch       = visPassParams.geometryPassData.geometryBatches[batchIdx];
		const BatchGPUData& batchGPUData = gpuBatches[batchIdx];

		lib::MTHandle<GeometryDrawMeshesDS> drawMeshesDS = graphBuilder.CreateDescriptorSet<GeometryDrawMeshesDS>(RENDERER_RESOURCE_NAME("GeometryDrawMeshesDS"));
		drawMeshesDS->u_visibleMeshlets      = passContext.visibleMeshlets;
		drawMeshesDS->u_visibleMeshletsCount = passContext.visibleMeshletsCount;
		drawMeshesDS->u_drawCommands = batchGPUData.drawCommands;

		if constexpr (passIdx == EGeometryVisPass::VisibleGeometryPass)
		{
			drawMeshesDS->u_occludedMeshlets                = batchGPUData.occludedMeshlets;
			drawMeshesDS->u_occludedMeshletsCount           = batchGPUData.occludedMeshletsCount;
			drawMeshesDS->u_occludedMeshletsDispatchCommand = batchGPUData.dispatchOccludedMeshletsCommand;
		}

		const rdr::PipelineStateID pipeline = CreatePipelineForBatch<passIdx>(visPassParams, passContext, batch);

		const Uint32 maxDrawsCount = batch.batchElementsNum;

		IndirectGeometryBatchDrawParams indirectDrawParams;
		indirectDrawParams.drawCommands      = batchGPUData.drawCommands;
		indirectDrawParams.drawCommandsCount = batchGPUData.drawCommandsCount;

		graphBuilder.AddSubpass(RG_DEBUG_NAME_FORMATTED("Batch Subpass ({})", GeometryPassTraits<passIdx>::GetPassName()),
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


static void DrawDisoccludedMeshlets(rg::RenderGraphBuilder& graphBuilder, const VisPassParams& visPassParams, const GeometryPassContext& passContext, lib::Span<const BatchGPUData> gpuBatches)
{
	SPT_PROFILER_FUNCTION();

	constexpr EGeometryVisPass passIdx = EGeometryVisPass::DisoccludedMeshletsPass;

	CreateRenderPass<passIdx>(graphBuilder, visPassParams, passContext);

	for (SizeType batchIdx = 0; batchIdx < visPassParams.geometryPassData.geometryBatches.size(); ++batchIdx)
	{
		const GeometryBatch& batch       = visPassParams.geometryPassData.geometryBatches[batchIdx];
		const BatchGPUData& batchGPUData = gpuBatches[batchIdx];

		lib::MTHandle<GeometryDrawMeshes_DisoccludedMeshletsPassDS> drawMeshesDS = graphBuilder.CreateDescriptorSet<GeometryDrawMeshes_DisoccludedMeshletsPassDS>(RENDERER_RESOURCE_NAME("GeometryDrawMeshes_DisoccludedMeshletsPassDS"));
		drawMeshesDS->u_visibleMeshlets       = passContext.visibleMeshlets;
		drawMeshesDS->u_visibleMeshletsCount  = passContext.visibleMeshletsCount;
		drawMeshesDS->u_occludedMeshlets      = batchGPUData.occludedMeshlets;
		drawMeshesDS->u_occludedMeshletsCount = batchGPUData.occludedMeshletsCount;

		const rdr::PipelineStateID pipeline = CreatePipelineForBatch<passIdx>(visPassParams, passContext, batch);

		IndirectGeometryBatchDrawParams indirectDrawParams;
		indirectDrawParams.drawCommands = batchGPUData.dispatchOccludedMeshletsCommand;

		graphBuilder.AddSubpass(RG_DEBUG_NAME("Batch Subpass"),
								rg::BindDescriptorSets(drawMeshesDS, batch.batchDS, GeometryManager::Get().GetGeometryDSState()),
								std::tie(indirectDrawParams),
								[indirectDrawParams, pipeline]
								(const lib::SharedRef<rdr::RenderContext>& renderContext, rdr::CommandRecorder& recorder)
								{
									recorder.BindGraphicsPipeline(pipeline);

									const rdr::BufferView& dispatchCommandView = indirectDrawParams.drawCommands->GetResource();

									recorder.DrawMeshTasksIndirect(dispatchCommandView.GetBuffer(),
																   dispatchCommandView.GetOffset(),
																   sizeof(DispatchOccludedBatchElementsCommand),
																   1u);
								});
	}
}


static void DrawGeometryVisibleLastFrame(rg::RenderGraphBuilder& graphBuilder, const VisPassParams& visPassParams, const GeometryPassContext& passContext, lib::Span<const BatchGPUData> gpuBatches)
{
	SPT_PROFILER_FUNCTION();

	CullBatchElements<EGeometryVisPass::VisibleGeometryPass>(graphBuilder, visPassParams, gpuBatches);
	DrawBatchElements<EGeometryVisPass::VisibleGeometryPass>(graphBuilder, visPassParams, passContext, gpuBatches);
}


static void DrawDisoccludedGeometry(rg::RenderGraphBuilder& graphBuilder, const VisPassParams& visPassParams, const GeometryPassContext& passContext, lib::Span<const BatchGPUData> gpuBatches)
{
	SPT_PROFILER_FUNCTION();

	CullBatchElements<EGeometryVisPass::DisoccludedGeometryPass>(graphBuilder, visPassParams, gpuBatches);
	DrawBatchElements<EGeometryVisPass::DisoccludedGeometryPass>(graphBuilder, visPassParams, passContext, gpuBatches);
	DrawDisoccludedMeshlets(graphBuilder, visPassParams, passContext, gpuBatches);
}

} // priv

static void RenderGeometryPrologue(rg::RenderGraphBuilder& graphBuilder, const VisPassParams& visPassParams, const GeometryPassContext& passContext)
{
	SPT_PROFILER_FUNCTION();

	graphBuilder.FillBuffer(RG_DEBUG_NAME("Reset Visible Meshlets Count"), passContext.visibleMeshletsCount, 0u, passContext.visibleMeshletsCount->GetSize(), 0u);
}


static void RenderGeometryVisPass(rg::RenderGraphBuilder& graphBuilder, const VisPassParams& visPassParams, const GeometryPassContext& passContext)
{
	SPT_PROFILER_FUNCTION();

	const ViewRenderingSpec& viewSpec = visPassParams.viewSpec;
	const RenderView& renderView      = viewSpec.GetRenderView();

	const lib::MTHandle<priv::VisCullingDS> cullingDS = priv::CreateCullingDS(visPassParams);

	const rg::BindDescriptorSetsScope geometryCullingDSScope(graphBuilder,
															 rg::BindDescriptorSets(StaticMeshUnifiedData::Get().GetUnifiedDataDS(),
																					mat::MaterialsUnifiedData::Get().GetMaterialsDS(),
																					renderView.GetRenderViewDS(),
																					cullingDS));

	const lib::DynamicArray<priv::BatchGPUData> gpuBatches = priv::BuildGPUBatches(graphBuilder, visPassParams);

	priv::DrawGeometryVisibleLastFrame(graphBuilder, visPassParams, passContext, gpuBatches);

	HiZ::CreateHierarchicalZ(graphBuilder, visPassParams.depth, visPassParams.hiZ->GetTexture());

	priv::DrawDisoccludedGeometry(graphBuilder, visPassParams, passContext, gpuBatches);

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

GeometryPassResult GeometryRenderer::RenderVisibility(rg::RenderGraphBuilder& graphBuilder, const VisPassParams& visPassParams)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(visPassParams.depth.IsValid());
	SPT_CHECK(visPassParams.hiZ.IsValid());
	SPT_CHECK(visPassParams.visibilityTexture.IsValid());

	const rg::RGBufferViewHandle visibleMeshlets = graphBuilder.AcquireExternalBufferView(m_visibleMeshlets->CreateFullView());
	const rg::RGBufferViewHandle visibleMeshletsCount = graphBuilder.AcquireExternalBufferView(m_visibleMeshletsCount->CreateFullView());

	geometry_rendering::GeometryPassContext passContext(visibleMeshlets, visibleMeshletsCount);

	geometry_rendering::RenderGeometryPrologue(graphBuilder, visPassParams, passContext);

	geometry_rendering::RenderGeometryVisPass(graphBuilder, visPassParams, passContext);

	GeometryPassResult result;
	result.visibleMeshlets = visibleMeshlets;

	return result;
}

} // spt::rsc
