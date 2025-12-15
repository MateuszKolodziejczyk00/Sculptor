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

struct GeometryPassContext
{
	GeometryPassContext(rg::RGBufferViewHandle visibleMeshlets, rg::RGBufferViewHandle visibleMeshletsCount)
		: visibleMeshlets(visibleMeshlets)
		, visibleMeshletsCount(visibleMeshletsCount)
	{ }

	rg::RGBufferViewHandle visibleMeshlets;
	rg::RGBufferViewHandle visibleMeshletsCount;
};

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


BEGIN_RG_NODE_PARAMETERS_STRUCT(IndirectGeometryBatchDrawParams)
	RG_BUFFER_VIEW(drawCommands,      rg::ERGBufferAccess::Read, rhi::EPipelineStage::DrawIndirect)
	RG_BUFFER_VIEW(drawCommandsCount, rg::ERGBufferAccess::Read, rhi::EPipelineStage::DrawIndirect)
END_RG_NODE_PARAMETERS_STRUCT();


static lib::MTHandle<VisCullingDS> CreateCullingDS(rg::RGTextureViewHandle hiZ, rg::RGTextureViewHandle historyHiZ)
{
	const Bool hasHistoryHiZ = historyHiZ.IsValid();

	VisCullingParams visCullingParams;
	visCullingParams.hiZResolution        = hiZ->GetResolution2D().cast<Real32>();
	visCullingParams.historyHiZResolution = hasHistoryHiZ ? historyHiZ->GetResolution2D().cast<Real32>() : math::Vector2f{};
	visCullingParams.hasHistoryHiZ        = hasHistoryHiZ;

	lib::MTHandle<VisCullingDS> visCullingDS = rdr::ResourcesManager::CreateDescriptorSetState<VisCullingDS>(RENDERER_RESOURCE_NAME("VisCullingDS"));
	visCullingDS->u_hiZTexture        = hiZ;
	visCullingDS->u_historyHiZTexture = historyHiZ;
	visCullingDS->u_visCullingParams  = visCullingParams;

	return visCullingDS;
}


namespace vis_buffer
{

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


template<EGeometryVisPass passIdx>
struct GeometryVisPassTraits
{
};

template<>
struct GeometryVisPassTraits<EGeometryVisPass::VisibleGeometryPass>
{
	using DrawMeshesDSType = GeometryDrawMeshes_VisibleGeometryPassDS;

	static constexpr const char* GetPassName() { return "Visible Geometry Pass"; }
};

template<>
struct GeometryVisPassTraits<EGeometryVisPass::DisoccludedGeometryPass>
{
	using DrawMeshesDSType = GeometryDrawMeshes_DisoccludedGeometryPassDS;

	static constexpr const char* GetPassName() { return "Disoccluded Geometry Pass"; }
};

template<>
struct GeometryVisPassTraits<EGeometryVisPass::DisoccludedMeshletsPass>
{
	using DrawMeshesDSType = GeometryDrawMeshes_DisoccludedMeshletsPassDS;

	static constexpr const char* GetPassName() { return "Disoccluded Meshlets Pass"; }
};


struct VisBatchGPUData
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


static VisBatchGPUData CreateVisGPUBatch(rg::RenderGraphBuilder& graphBuilder, const GeometryBatch& batch)
{
	SPT_PROFILER_FUNCTION();

	VisBatchGPUData batchGPUData;

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


BEGIN_SHADER_STRUCT(GeometryVisibilityBufferPermutation)
	SHADER_STRUCT_FIELD(GeometryBatchPermutation, BATCH)
	SHADER_STRUCT_FIELD(Int32,                    GEOMETRY_PASS_IDX)
END_SHADER_STRUCT();


GRAPHICS_PSO(GeometryVisibilityBufferPSO)
{
	TASK_SHADER("Sculptor/GeometryRendering/GeometryVisibilityBuffer.hlsl", GeometryVisibility_TS);
	MESH_SHADER("Sculptor/GeometryRendering/GeometryVisibilityBuffer.hlsl", GeometryVisibility_MS);
	FRAGMENT_SHADER("Sculptor/GeometryRendering/GeometryVisibilityBuffer.hlsl", GeometryVisibility_FS);

	PERMUTATION_DOMAIN(GeometryVisibilityBufferPermutation);
};


template<EGeometryVisPass passIdx>
static rdr::PipelineStateID CreatePipelineForBatch(const VisPassParams& visPassParams, const GeometryBatch& batch)
{
	SPT_PROFILER_FUNCTION();

	const rhi::GraphicsPipelineDefinition pipelineDef
	{
		.primitiveTopology = rhi::EPrimitiveTopology::TriangleList,
		.rasterizationDefinition =
		{
			.cullMode = batch.permutation.DOUBLE_SIDED ? rhi::ECullMode::None : rhi::ECullMode::Back,
		},
		.renderTargetsDefinition =
		{
			.colorRTsDefinition =
			{
				rhi::ColorRenderTargetDefinition
				{
					.format         = visPassParams.visibilityTexture->GetFormat(),
					.colorBlendType = rhi::ERenderTargetBlendType::Disabled,
					.alphaBlendType = rhi::ERenderTargetBlendType::Disabled,
				}
			},
			.depthRTDefinition = rhi::DepthRenderTargetDefinition
			{
				.format = visPassParams.depth->GetFormat(),
				.depthCompareOp = rhi::ECompareOp::Greater,
			}
		}
	};

	GeometryVisibilityBufferPermutation permutation;
	permutation.BATCH             = batch.permutation;
	permutation.GEOMETRY_PASS_IDX = static_cast<Int32>(passIdx);

	return GeometryVisibilityBufferPSO::GetPermutation(pipelineDef, permutation);
}


lib::DynamicArray<VisBatchGPUData> BuildGPUBatches(rg::RenderGraphBuilder& graphBuilder, const VisPassParams& visPassParams)
{
	SPT_PROFILER_FUNCTION();

	lib::DynamicArray<VisBatchGPUData> batchesGPUData;
	batchesGPUData.reserve(visPassParams.geometryPassData.geometryBatches.size());

	for (const GeometryBatch& batch : visPassParams.geometryPassData.geometryBatches)
	{
		VisBatchGPUData batchGPUData = CreateVisGPUBatch(graphBuilder, batch);
		batchesGPUData.emplace_back(std::move(batchGPUData));
	}

	return batchesGPUData;
}


BEGIN_SHADER_STRUCT(CullBatchElementsPermutation)
	SHADER_STRUCT_FIELD(Int32, GEOMETRY_PASS_IDX)
END_SHADER_STRUCT();


COMPUTE_PSO(CullBatchElementsPSO)
{
	COMPUTE_SHADER("Sculptor/GeometryRendering/Geometry_CullSubmeshes.hlsl", CullSubmeshesCS);

	PERMUTATION_DOMAIN(CullBatchElementsPermutation);

	PRESET(passes)[2];

	static void PrecachePSOs(rdr::PSOCompilerInterface& compiler, const rdr::PSOPrecacheParams& params)
	{
		for (Int32 passIdx = 0; passIdx < 2; ++passIdx)
		{
			CullBatchElementsPermutation permutation;
			permutation.GEOMETRY_PASS_IDX = passIdx;
			passes[passIdx] = CompilePermutation(compiler, permutation);
		}
	}
};


template<EGeometryVisPass passIdx>
void CullBatchElements(rg::RenderGraphBuilder& graphBuilder, const VisPassParams& visPassParams, lib::Span<const VisBatchGPUData> gpuBatches)
{
	SPT_PROFILER_FUNCTION();

	SPT_RG_DIAGNOSTICS_SCOPE(graphBuilder, "Geometry Culling");

	static_assert(passIdx == EGeometryVisPass::VisibleGeometryPass || passIdx == EGeometryVisPass::DisoccludedGeometryPass);

	using CullSubmeshesDS = std::conditional_t<passIdx == EGeometryVisPass::VisibleGeometryPass, GeometryCullSubmeshes_VisibleGeometryPassDS, GeometryCullSubmeshes_DisoccludedGeometryPassDS>;

	for(SizeType batchIdx = 0u; batchIdx < visPassParams.geometryPassData.geometryBatches.size(); ++batchIdx)
	{
		const GeometryBatch& batch          = visPassParams.geometryPassData.geometryBatches[batchIdx];
		const VisBatchGPUData& batchGPUData = gpuBatches[batchIdx];

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

			graphBuilder.Dispatch(RG_DEBUG_NAME_FORMATTED("Cull Submeshes ({})", GeometryVisPassTraits<passIdx>::GetPassName()),
								  CullBatchElementsPSO::passes[static_cast<Int32>(passIdx)],
								  dispatchGroups,
								  rg::BindDescriptorSets(batch.batchDS, cullSubmeshesDS));
		}
		else if constexpr (passIdx == EGeometryVisPass::DisoccludedGeometryPass)
		{
			graphBuilder.DispatchIndirect(RG_DEBUG_NAME_FORMATTED("Cull Submeshes ({})", GeometryVisPassTraits<passIdx>::GetPassName()),
										  CullBatchElementsPSO::passes[static_cast<Int32>(passIdx)],
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
	
	graphBuilder.RenderPass(RG_DEBUG_NAME_FORMATTED("Visibility Pass ({})", GeometryVisPassTraits<passIdx>::GetPassName()),
							renderPassDef,
							rg::EmptyDescriptorSets(),
							[resolution](const lib::SharedRef<rdr::RenderContext>& renderContext, rdr::CommandRecorder& recorder)
							{
								recorder.SetViewport(math::AlignedBox2f(math::Vector2f(0.f, 0.f), resolution.cast<Real32>()), 0.f, 1.f);
								recorder.SetScissor(math::AlignedBox2u(math::Vector2u(0, 0), resolution));
							});
}


template<EGeometryVisPass passIdx>
static void DrawBatchElements(rg::RenderGraphBuilder& graphBuilder, const VisPassParams& visPassParams, const GeometryPassContext& passContext, lib::Span<const VisBatchGPUData> gpuBatches)
{
	SPT_PROFILER_FUNCTION();

	static_assert(passIdx == EGeometryVisPass::VisibleGeometryPass || passIdx == EGeometryVisPass::DisoccludedGeometryPass);

	using GeometryDrawMeshesDS = typename GeometryVisPassTraits<passIdx>::DrawMeshesDSType;

	CreateRenderPass<passIdx>(graphBuilder, visPassParams, passContext);

	for (SizeType batchIdx = 0; batchIdx < visPassParams.geometryPassData.geometryBatches.size(); ++batchIdx)
	{
		const GeometryBatch& batch       = visPassParams.geometryPassData.geometryBatches[batchIdx];
		const VisBatchGPUData& batchGPUData = gpuBatches[batchIdx];

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

		const rdr::PipelineStateID pipeline = CreatePipelineForBatch<passIdx>(visPassParams, batch);

		const Uint32 maxDrawsCount = batch.batchElementsNum;

		IndirectGeometryBatchDrawParams indirectDrawParams;
		indirectDrawParams.drawCommands      = batchGPUData.drawCommands;
		indirectDrawParams.drawCommandsCount = batchGPUData.drawCommandsCount;

		graphBuilder.AddSubpass(RG_DEBUG_NAME_FORMATTED("Batch Subpass ({})", GeometryVisPassTraits<passIdx>::GetPassName()),
								rg::BindDescriptorSets(drawMeshesDS, batch.batchDS),
								std::tie(indirectDrawParams),
								[indirectDrawParams, pipeline, maxDrawsCount]
								(const lib::SharedRef<rdr::RenderContext>& renderContext, rdr::CommandRecorder& recorder)
								{
									recorder.BindGraphicsPipeline(pipeline);

									const rdr::BufferView& drawCommandsView      = *indirectDrawParams.drawCommands->GetResource();
									const rdr::BufferView& drawCommandsCountView = *indirectDrawParams.drawCommandsCount->GetResource();

									recorder.DrawMeshTasksIndirectCount(drawCommandsView.GetBuffer(),
																		drawCommandsView.GetOffset(),
																		sizeof(GeometryDrawMeshTaskCommand),
																		drawCommandsCountView.GetBuffer(),
																		drawCommandsCountView.GetOffset(),
																		maxDrawsCount);
								});
	}
}


static void DrawDisoccludedMeshlets(rg::RenderGraphBuilder& graphBuilder, const VisPassParams& visPassParams, const GeometryPassContext& passContext, lib::Span<const VisBatchGPUData> gpuBatches)
{
	SPT_PROFILER_FUNCTION();

	constexpr EGeometryVisPass passIdx = EGeometryVisPass::DisoccludedMeshletsPass;

	CreateRenderPass<passIdx>(graphBuilder, visPassParams, passContext);

	for (SizeType batchIdx = 0; batchIdx < visPassParams.geometryPassData.geometryBatches.size(); ++batchIdx)
	{
		const GeometryBatch& batch       = visPassParams.geometryPassData.geometryBatches[batchIdx];
		const VisBatchGPUData& batchGPUData = gpuBatches[batchIdx];

		lib::MTHandle<GeometryDrawMeshes_DisoccludedMeshletsPassDS> drawMeshesDS = graphBuilder.CreateDescriptorSet<GeometryDrawMeshes_DisoccludedMeshletsPassDS>(RENDERER_RESOURCE_NAME("GeometryDrawMeshes_DisoccludedMeshletsPassDS"));
		drawMeshesDS->u_visibleMeshlets       = passContext.visibleMeshlets;
		drawMeshesDS->u_visibleMeshletsCount  = passContext.visibleMeshletsCount;
		drawMeshesDS->u_occludedMeshlets      = batchGPUData.occludedMeshlets;
		drawMeshesDS->u_occludedMeshletsCount = batchGPUData.occludedMeshletsCount;

		const rdr::PipelineStateID pipeline = CreatePipelineForBatch<passIdx>(visPassParams, batch);

		IndirectGeometryBatchDrawParams indirectDrawParams;
		indirectDrawParams.drawCommands = batchGPUData.dispatchOccludedMeshletsCommand;

		graphBuilder.AddSubpass(RG_DEBUG_NAME("Batch Subpass"),
								rg::BindDescriptorSets(drawMeshesDS, batch.batchDS),
								std::tie(indirectDrawParams),
								[indirectDrawParams, pipeline]
								(const lib::SharedRef<rdr::RenderContext>& renderContext, rdr::CommandRecorder& recorder)
								{
									recorder.BindGraphicsPipeline(pipeline);

									const rdr::BufferView& dispatchCommandView = indirectDrawParams.drawCommands->GetResourceRef();

									recorder.DrawMeshTasksIndirect(dispatchCommandView.GetBuffer(),
																   dispatchCommandView.GetOffset(),
																   sizeof(DispatchOccludedBatchElementsCommand),
																   1u);
								});
	}
}


static void DrawGeometryVisibleLastFrame(rg::RenderGraphBuilder& graphBuilder, const VisPassParams& visPassParams, const GeometryPassContext& passContext, lib::Span<const VisBatchGPUData> gpuBatches)
{
	SPT_PROFILER_FUNCTION();

	CullBatchElements<EGeometryVisPass::VisibleGeometryPass>(graphBuilder, visPassParams, gpuBatches);
	DrawBatchElements<EGeometryVisPass::VisibleGeometryPass>(graphBuilder, visPassParams, passContext, gpuBatches);
}


static void DrawDisoccludedGeometry(rg::RenderGraphBuilder& graphBuilder, const VisPassParams& visPassParams, const GeometryPassContext& passContext, lib::Span<const VisBatchGPUData> gpuBatches)
{
	SPT_PROFILER_FUNCTION();

	CullBatchElements<EGeometryVisPass::DisoccludedGeometryPass>(graphBuilder, visPassParams, gpuBatches);
	DrawBatchElements<EGeometryVisPass::DisoccludedGeometryPass>(graphBuilder, visPassParams, passContext, gpuBatches);
	DrawDisoccludedMeshlets(graphBuilder, visPassParams, passContext, gpuBatches);
}


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

	const lib::MTHandle<VisCullingDS> cullingDS = CreateCullingDS(visPassParams.hiZ, visPassParams.historyHiZ);

	const rg::BindDescriptorSetsScope geometryCullingDSScope(graphBuilder,
															 rg::BindDescriptorSets(renderView.GetRenderViewDS(),
																					cullingDS));

	const lib::DynamicArray<VisBatchGPUData> gpuBatches = BuildGPUBatches(graphBuilder, visPassParams);

	DrawGeometryVisibleLastFrame(graphBuilder, visPassParams, passContext, gpuBatches);

	HiZ::CreateHierarchicalZ(graphBuilder, visPassParams.depth, visPassParams.hiZ->GetTexture());

	DrawDisoccludedGeometry(graphBuilder, visPassParams, passContext, gpuBatches);

	HiZ::CreateHierarchicalZ(graphBuilder, visPassParams.depth, visPassParams.hiZ->GetTexture());
}

} // vis_buffer

namespace oit
{

enum class EOITPass
{
	HashPass,
	ABufferPass,
	Num
};


template<EOITPass passIdx>
struct OITPassTraits
{
};

template<>
struct OITPassTraits<EOITPass::HashPass>
{
	//using DrawMeshesDSType = GeometryDrawMeshes_VisibleGeometryPassDS;

	static constexpr const char* GetPassName() { return "Hash Pass"; }
};

template<>
struct OITPassTraits<EOITPass::ABufferPass>
{
	//using DrawMeshesDSType = GeometryDrawMeshes_DisoccludedGeometryPassDS;

	static constexpr const char* GetPassName() { return "A-Buffer Pass"; }
};


struct OITBatchGPUData
{
	const GeometryBatch& def;

	rg::RGBufferViewHandle drawCommands;
	rg::RGBufferViewHandle drawCommandsCount;
};


static OITBatchGPUData CreateOITGPUBatch(rg::RenderGraphBuilder& graphBuilder, const GeometryBatch& batch)
{
	SPT_PROFILER_FUNCTION();

	OITBatchGPUData batchGPUData{ batch };

	rhi::BufferDefinition drawCommandsBufferDef;
	drawCommandsBufferDef.size  = sizeof(GeometryDrawMeshTaskCommand) * batch.batchElementsNum;
	drawCommandsBufferDef.usage = lib::Flags(rhi::EBufferUsage::Storage, rhi::EBufferUsage::Indirect);
	const rg::RGBufferViewHandle drawCommands = graphBuilder.CreateBufferView(RG_DEBUG_NAME("Draw Mesh Commands"), drawCommandsBufferDef, rhi::EMemoryUsage::GPUOnly);

	rhi::BufferDefinition drawCommandsCountBufferDef;
	drawCommandsCountBufferDef.size  = sizeof(Uint32);
	drawCommandsCountBufferDef.usage = lib::Flags(rhi::EBufferUsage::Storage, rhi::EBufferUsage::Indirect, rhi::EBufferUsage::TransferDst);
	const rg::RGBufferViewHandle drawCommandsCount = graphBuilder.CreateBufferView(RG_DEBUG_NAME("Draw Mesh Commands Count"), drawCommandsCountBufferDef, rhi::EMemoryUsage::GPUOnly);
	batchGPUData.drawCommands                         = drawCommands;
	batchGPUData.drawCommandsCount                    = drawCommandsCount;

	return batchGPUData;
}


template<EOITPass passIdx>
static rdr::PipelineStateID CreatePipelineForBatch(const GeometryOITPassParams& oitPassParams, const GeometryBatch& batch)
{
	SPT_PROFILER_FUNCTION();

	return rdr::PipelineStateID{};
}


DS_BEGIN(OITCullBatchElementsDS, rg::RGDescriptorSetState<OITCullBatchElementsDS>)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<GeometryDrawMeshTaskCommand>), u_drawCommands)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<Uint32>),                      u_drawCommandsCount)
DS_END();


static rdr::PipelineStateID CreateCullBatchElementsPipeline()
{
	const rdr::ShaderID cullBatchElementsShader = rdr::ResourcesManager::CreateShader("Sculptor/GeometryRendering/OIT_CullBatchElements.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "CullBatchElementsCS"));
	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("OIT_CullBatchElementsPipeline"), cullBatchElementsShader);
}


static void CullBatchElements(rg::RenderGraphBuilder& graphBuilder, const GeometryOITPassParams& oitPassParams, const OITBatchGPUData& oitBatch)
{
	SPT_PROFILER_FUNCTION();

	graphBuilder.FillFullBuffer(RG_DEBUG_NAME("Clear Draw Commands Num"), oitBatch.drawCommandsCount, 0u);

	lib::MTHandle<OITCullBatchElementsDS> cullBatchElementsDS = graphBuilder.CreateDescriptorSet<OITCullBatchElementsDS>(RENDERER_RESOURCE_NAME("OITCullBatchElementsDS"));
	cullBatchElementsDS->u_drawCommands      = oitBatch.drawCommands;
	cullBatchElementsDS->u_drawCommandsCount = oitBatch.drawCommandsCount;

	static const rdr::PipelineStateID pipeline = CreateCullBatchElementsPipeline();

	graphBuilder.Dispatch(RG_DEBUG_NAME("Cull Batch Elements"),
						  pipeline,
						  math::Utils::DivideCeil(oitBatch.def.batchElementsNum, 64u),
						  rg::BindDescriptorSets(oitBatch.def.batchDS, cullBatchElementsDS));
}


rg::RGTextureViewHandle RenderHashPass(rg::RenderGraphBuilder& graphBuilder, const GeometryOITPassParams& oitPassParams, const OITBatchGPUData& oitBatch)
{
	SPT_PROFILER_FUNCTION();

	const math::Vector2u resolution = oitPassParams.viewSpec.GetRenderView().GetRenderingHalfRes();

	const rg::RGTextureViewHandle hashTexture = graphBuilder.CreateTextureView(RG_DEBUG_NAME("OIT Hash Texture"), rg::TextureDef(resolution, rhi::EFragmentFormat::R32_U_Int), rhi::EMemoryUsage::GPUOnly);

	graphBuilder.ClearTexture(RG_DEBUG_NAME("Clear Hash Texture"), hashTexture, rhi::ClearColor(0u, 0u, 0u, 0u));

	rg::RGRenderPassDefinition renderPassDef(math::Vector2i(0, 0), resolution);

	rg::RGRenderTargetDef depthRTDef;
	depthRTDef.textureView    = oitPassParams.depth;
	depthRTDef.loadOperation  = rhi::ERTLoadOperation::Load;
	depthRTDef.storeOperation = rhi::ERTStoreOperation::Store;
	renderPassDef.SetDepthRenderTarget(depthRTDef);

	IndirectGeometryBatchDrawParams indirectDrawParams;
	indirectDrawParams.drawCommands      = oitBatch.drawCommands;
	indirectDrawParams.drawCommandsCount = oitBatch.drawCommandsCount;

	const rdr::PipelineStateID pipeline = CreatePipelineForBatch<EOITPass::HashPass>(oitPassParams, oitBatch.def);

	const Uint32 maxDrawsCount = oitBatch.def.batchElementsNum;

	graphBuilder.RenderPass(RG_DEBUG_NAME("OIT Hash Pass"),
							renderPassDef,
							rg::EmptyDescriptorSets(),
							std::tie(indirectDrawParams),
							[resolution, pipeline, indirectDrawParams, maxDrawsCount](const lib::SharedRef<rdr::RenderContext>& renderContext, rdr::CommandRecorder& recorder)
							{
								recorder.SetViewport(math::AlignedBox2f(math::Vector2f(0.f, 0.f), resolution.cast<Real32>()), 0.f, 1.f);
								recorder.SetScissor(math::AlignedBox2u(math::Vector2u(0, 0), resolution));

								recorder.BindGraphicsPipeline(pipeline);

								const rdr::BufferView& drawCommandsView      = *indirectDrawParams.drawCommands->GetResource();
								const rdr::BufferView& drawCommandsCountView = *indirectDrawParams.drawCommandsCount->GetResource();

								recorder.DrawMeshTasksIndirectCount(drawCommandsView.GetBuffer(),
																	drawCommandsView.GetOffset(),
																	sizeof(GeometryDrawMeshTaskCommand),
																	drawCommandsCountView.GetBuffer(),
																	drawCommandsCountView.GetOffset(),
																	maxDrawsCount);

							});

	return hashTexture;
}


static void RenderGeometryPrologue(rg::RenderGraphBuilder& graphBuilder, const GeometryOITPassParams& oitPassParams, const GeometryPassContext& passContext)
{
	SPT_PROFILER_FUNCTION();

	graphBuilder.FillBuffer(RG_DEBUG_NAME("Reset Visible Meshlets Count"), passContext.visibleMeshletsCount, 0u, passContext.visibleMeshletsCount->GetSize(), 0u);
}


static void RenderGeometryVisPass(rg::RenderGraphBuilder& graphBuilder, const GeometryOITPassParams& oitPassParams, const GeometryPassContext& passContext)
{
	SPT_PROFILER_FUNCTION();

	const ViewRenderingSpec& viewSpec = oitPassParams.viewSpec;
	const RenderView& renderView      = viewSpec.GetRenderView();

	const lib::MTHandle<VisCullingDS> cullingDS = CreateCullingDS(oitPassParams.hiZ, nullptr);

	const rg::BindDescriptorSetsScope geometryCullingDSScope(graphBuilder,
															 rg::BindDescriptorSets(renderView.GetRenderViewDS(),
																					cullingDS));

	SPT_CHECK(oitPassParams.geometryPassData.geometryBatches.size() == 1u);

	const OITBatchGPUData gpuBatch = CreateOITGPUBatch(graphBuilder, oitPassParams.geometryPassData.geometryBatches[0]);

	CullBatchElements(graphBuilder, oitPassParams, gpuBatch);

	SPT_MAYBE_UNUSED
	const rg::RGTextureViewHandle hashTexture = RenderHashPass(graphBuilder, oitPassParams, gpuBatch);

	// TODO: alloc A-Buffer draws

	// TODO: Draw A-Buffer pass

	// TODO: sort A-Buffer

	// TODO: Shading + blending

	// TODO: resolve


}

} // oit

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

	SPT_RG_DIAGNOSTICS_SCOPE(graphBuilder, "Visibliity Buffer");

	SPT_CHECK(visPassParams.depth.IsValid());
	SPT_CHECK(visPassParams.hiZ.IsValid());
	SPT_CHECK(visPassParams.visibilityTexture.IsValid());

	const rg::RGBufferViewHandle visibleMeshlets      = graphBuilder.AcquireExternalBufferView(m_visibleMeshlets->GetFullView());
	const rg::RGBufferViewHandle visibleMeshletsCount = graphBuilder.AcquireExternalBufferView(m_visibleMeshletsCount->GetFullView());

	geometry_rendering::GeometryPassContext passContext(visibleMeshlets, visibleMeshletsCount);

	geometry_rendering::vis_buffer::RenderGeometryPrologue(graphBuilder, visPassParams, passContext);

	geometry_rendering::vis_buffer::RenderGeometryVisPass(graphBuilder, visPassParams, passContext);

	GeometryPassResult result;
	result.visibleMeshlets = visibleMeshlets;

	return result;
}

GeometryOITResult GeometryRenderer::RenderTransparentGeometry(rg::RenderGraphBuilder& graphBuilder, const GeometryOITPassParams& oitPassParams)
{
	SPT_PROFILER_FUNCTION();

	SPT_RG_DIAGNOSTICS_SCOPE(graphBuilder, "Geometry OIT");

	const rg::RGBufferViewHandle visibleMeshlets      = graphBuilder.AcquireExternalBufferView(m_visibleMeshlets->GetFullView());
	const rg::RGBufferViewHandle visibleMeshletsCount = graphBuilder.AcquireExternalBufferView(m_visibleMeshletsCount->GetFullView());

	geometry_rendering::GeometryPassContext passContext(visibleMeshlets, visibleMeshletsCount);

	geometry_rendering::oit::RenderGeometryPrologue(graphBuilder, oitPassParams, passContext);

	geometry_rendering::oit::RenderGeometryVisPass(graphBuilder, oitPassParams, passContext);

	GeometryOITResult result;
	// TODO

	return result;

}

} // spt::rsc
