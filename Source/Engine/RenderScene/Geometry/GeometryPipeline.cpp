#include "GeometryPipeline.h"
#include "ResourcesManager.h"
#include "RenderGraphBuilder.h"
#include "RGDescriptorSetState.h"
#include "DescriptorSetBindings/RWBufferBinding.h"
#include "DescriptorSetBindings/ConstantBufferBinding.h"
#include "DescriptorSetBindings/SRVTextureBinding.h"
#include "View/ViewRenderingSpec.h"
#include "SceneRenderer/RenderStages/Utils/hiZRenderer.h"


namespace spt::rsc::gp
{

struct GeometryPassContext
{
	explicit GeometryPassContext(GeometryRenderingPipeline& inPipeline)
		: pipeline(inPipeline)
	{ }

	GeometryRenderingPipeline& pipeline;
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


BEGIN_SHADER_STRUCT(GeometryCullingParams)
	SHADER_STRUCT_FIELD(math::Vector2f, hiZResolution)
	SHADER_STRUCT_FIELD(math::Vector2f, historyHiZResolution)
	SHADER_STRUCT_FIELD(Bool,           hasHistoryHiZ)
END_SHADER_STRUCT();


DS_BEGIN(GeometryCullingDS, rg::RGDescriptorSetState<GeometryCullingDS>)
	DS_BINDING(BINDING_TYPE(gfx::OptionalSRVTexture2DBinding<Real32>),                              u_hiZTexture) // valid only for 2nd pass
	DS_BINDING(BINDING_TYPE(gfx::OptionalSRVTexture2DBinding<Real32>),                              u_historyHiZTexture)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::LinearMinClampToEdge>), u_hiZSampler)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<GeometryCullingParams>),                     u_visCullingParams)
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


static lib::MTHandle<GeometryCullingDS> CreateCullingDS(rg::RGTextureViewHandle hiZ, rg::RGTextureViewHandle historyHiZ)
{
	const Bool hasHistoryHiZ = historyHiZ.IsValid();

	GeometryCullingParams visCullingParams;
	visCullingParams.hiZResolution        = hiZ->GetResolution2D().cast<Real32>();
	visCullingParams.historyHiZResolution = hasHistoryHiZ ? historyHiZ->GetResolution2D().cast<Real32>() : math::Vector2f{};
	visCullingParams.hasHistoryHiZ        = hasHistoryHiZ;

	lib::MTHandle<GeometryCullingDS> visCullingDS = rdr::ResourcesManager::CreateDescriptorSetState<GeometryCullingDS>(RENDERER_RESOURCE_NAME("VisCullingDS"));
	visCullingDS->u_hiZTexture        = hiZ;
	visCullingDS->u_historyHiZTexture = historyHiZ;
	visCullingDS->u_visCullingParams  = visCullingParams;

	return visCullingDS;
}


DS_BEGIN(GeometryDrawMeshes_VisibleGeometryPassDS, rg::RGDescriptorSetState<GeometryDrawMeshes_VisibleGeometryPassDS>)
	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<GeometryDrawMeshTaskCommand>), u_drawCommands)
	
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<OccludedMeshletData>),                  u_occludedMeshlets)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<Uint32>),                               u_occludedMeshletsCount)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<DispatchOccludedBatchElementsCommand>), u_occludedMeshletsDispatchCommand)
DS_END();


DS_BEGIN(GeometryDrawMeshes_DisoccludedGeometryPassDS, rg::RGDescriptorSetState<GeometryDrawMeshes_DisoccludedGeometryPassDS>)
	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<GeometryDrawMeshTaskCommand>), u_drawCommands)
DS_END();


DS_BEGIN(GeometryDrawMeshes_DisoccludedMeshletsPassDS, rg::RGDescriptorSetState<GeometryDrawMeshes_DisoccludedMeshletsPassDS>)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<OccludedMeshletData>),       u_occludedMeshlets)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<Uint32>),                    u_occludedMeshletsCount)
DS_END();


template<EGeometryPass passIdx>
struct GeometryPassTraits
{
};

template<>
struct GeometryPassTraits<EGeometryPass::VisibleGeometryPass>
{
	using DrawMeshesDSType = GeometryDrawMeshes_VisibleGeometryPassDS;

	static constexpr const char* GetPassName() { return "Visible Geometry Pass"; }
};

template<>
struct GeometryPassTraits<EGeometryPass::DisoccludedGeometryPass>
{
	using DrawMeshesDSType = GeometryDrawMeshes_DisoccludedGeometryPassDS;

	static constexpr const char* GetPassName() { return "Disoccluded Geometry Pass"; }
};

template<>
struct GeometryPassTraits<EGeometryPass::DisoccludedMeshletsPass>
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
	batchGPUData.drawCommands                         = graphBuilder.CreateStorageBufferView(RG_DEBUG_NAME("Draw Mesh Commands"), sizeof(GeometryDrawMeshTaskCommand) * batch.batchElementsNum);;
	batchGPUData.drawCommandsCount                    = graphBuilder.CreateStorageBufferView(RG_DEBUG_NAME("Draw Mesh Commands Count"), sizeof(Uint32));
	batchGPUData.occludedBatchElements                = graphBuilder.CreateStorageBufferView(RG_DEBUG_NAME("Occluded Batch Elements"), sizeof(OccludedBatchElement) * batch.batchElementsNum);
	batchGPUData.occludedBatchElementsCount           = graphBuilder.CreateStorageBufferView(RG_DEBUG_NAME("Occluded Batch Elements Count"), sizeof(Uint32));
	batchGPUData.dispatchOccludedBatchElementsCommand = graphBuilder.CreateStorageBufferView(RG_DEBUG_NAME("Dispatch Occluded Batch Elements Command"), sizeof(DispatchOccludedBatchElementsCommand));
	batchGPUData.occludedMeshlets                     = graphBuilder.CreateStorageBufferView(RG_DEBUG_NAME("Occluded Meshlets"), sizeof(OccludedMeshletData) * batch.batchMeshletsNum);
	batchGPUData.occludedMeshletsCount                = graphBuilder.CreateStorageBufferView(RG_DEBUG_NAME("Occluded Meshlets Count"), sizeof(Uint32));
	batchGPUData.dispatchOccludedMeshletsCommand      = graphBuilder.CreateStorageBufferView(RG_DEBUG_NAME("Dispatch Occluded Meshlets Command"), sizeof(DispatchOccludedBatchElementsCommand));

	graphBuilder.MemZeroBuffer(batchGPUData.occludedBatchElementsCount);
	graphBuilder.MemZeroBuffer(batchGPUData.occludedMeshletsCount);
	graphBuilder.MemZeroBuffer(batchGPUData.dispatchOccludedMeshletsCommand);

	return batchGPUData;
}


lib::DynamicArray<BatchGPUData> BuildGPUBatches(rg::RenderGraphBuilder& graphBuilder, const GeometryPassParams& geometryPassParams)
{
	SPT_PROFILER_FUNCTION();

	lib::DynamicArray<BatchGPUData> batchesGPUData;
	batchesGPUData.reserve(geometryPassParams.geometryPassData.geometryBatches.size());

	for (const GeometryBatch& batch : geometryPassParams.geometryPassData.geometryBatches)
	{
		BatchGPUData batchGPUData = CreateGPUBatch(graphBuilder, batch);
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


template<EGeometryPass passIdx>
void CullBatchElements(rg::RenderGraphBuilder& graphBuilder, const GeometryPassParams& geometryPassParams, lib::Span<const BatchGPUData> gpuBatches)
{
	SPT_PROFILER_FUNCTION();

	SPT_RG_DIAGNOSTICS_SCOPE(graphBuilder, "Geometry Culling");

	static_assert(passIdx == EGeometryPass::VisibleGeometryPass || passIdx == EGeometryPass::DisoccludedGeometryPass);

	using CullSubmeshesDS = std::conditional_t<passIdx == EGeometryPass::VisibleGeometryPass, GeometryCullSubmeshes_VisibleGeometryPassDS, GeometryCullSubmeshes_DisoccludedGeometryPassDS>;

	for(SizeType batchIdx = 0u; batchIdx < geometryPassParams.geometryPassData.geometryBatches.size(); ++batchIdx)
	{
		const GeometryBatch& batch       = geometryPassParams.geometryPassData.geometryBatches[batchIdx];
		const BatchGPUData& batchGPUData = gpuBatches[batchIdx];

		graphBuilder.FillFullBuffer(RG_DEBUG_NAME("Initialize Draw Commands Count"), batchGPUData.drawCommandsCount, 0u);

		const lib::MTHandle<CullSubmeshesDS> cullSubmeshesDS = graphBuilder.CreateDescriptorSet<CullSubmeshesDS>(RENDERER_RESOURCE_NAME("CullSubmeshesDS"));
		cullSubmeshesDS->u_drawCommands               = batchGPUData.drawCommands;
		cullSubmeshesDS->u_drawCommandsCount          = batchGPUData.drawCommandsCount;
		cullSubmeshesDS->u_occludedBatchElements      = batchGPUData.occludedBatchElements;
		cullSubmeshesDS->u_occludedBatchElementsCount = batchGPUData.occludedBatchElementsCount;
		if constexpr (passIdx == EGeometryPass::VisibleGeometryPass)
		{
			cullSubmeshesDS->u_dispatchOccludedElementsCommand = batchGPUData.dispatchOccludedBatchElementsCommand;
		}

		if constexpr (passIdx == EGeometryPass::VisibleGeometryPass)
		{
			const Uint32 groupSize = 64u;
			const Uint32 dispatchGroups = math::Utils::DivideCeil(batch.batchElementsNum, groupSize);

			graphBuilder.Dispatch(RG_DEBUG_NAME_FORMATTED("Cull Submeshes ({})", GeometryPassTraits<passIdx>::GetPassName()),
								  CullBatchElementsPSO::passes[static_cast<Int32>(passIdx)],
								  dispatchGroups,
								  rg::BindDescriptorSets(batch.batchDS, cullSubmeshesDS));
		}
		else if constexpr (passIdx == EGeometryPass::DisoccludedGeometryPass)
		{
			graphBuilder.DispatchIndirect(RG_DEBUG_NAME_FORMATTED("Cull Submeshes ({})", GeometryPassTraits<passIdx>::GetPassName()),
										  CullBatchElementsPSO::passes[static_cast<Int32>(passIdx)],
										  batchGPUData.dispatchOccludedBatchElementsCommand,
										  0u,
										  rg::BindDescriptorSets(batch.batchDS, cullSubmeshesDS));
		}
	}
}


template<EGeometryPass passIdx>
static void CreateRenderPass(rg::RenderGraphBuilder& graphBuilder, const GeometryPassParams& geometryPassParams, const GeometryPassContext& passContext)
{
	SPT_PROFILER_FUNCTION();

	const RenderPassDefinition renderPassDef = passContext.pipeline.CreateRenderPassDefinition(graphBuilder, geometryPassParams, passIdx);
	
	graphBuilder.RenderPass(RG_DEBUG_NAME_FORMATTED("Geometry Pass ({})", GeometryPassTraits<passIdx>::GetPassName()),
							renderPassDef,
							rg::BindDescriptorSets(renderPassDef.perPassDS),
							[resolution = renderPassDef.resolution](const lib::SharedRef<rdr::RenderContext>& renderContext, rdr::CommandRecorder& recorder)
							{
								recorder.SetViewport(math::AlignedBox2f(math::Vector2f(0.f, 0.f), resolution.cast<Real32>()), 0.f, 1.f);
								recorder.SetScissor(math::AlignedBox2u(math::Vector2u(0, 0), resolution));
							});
}


template<EGeometryPass passIdx>
static void DrawBatchElements(rg::RenderGraphBuilder& graphBuilder, const GeometryPassParams& geometryPassParams, const GeometryPassContext& passContext, lib::Span<const BatchGPUData> gpuBatches)
{
	SPT_PROFILER_FUNCTION();

	static_assert(passIdx == EGeometryPass::VisibleGeometryPass || passIdx == EGeometryPass::DisoccludedGeometryPass);

	using GeometryDrawMeshesDS = typename GeometryPassTraits<passIdx>::DrawMeshesDSType;

	CreateRenderPass<passIdx>(graphBuilder, geometryPassParams, passContext);

	for (SizeType batchIdx = 0; batchIdx < geometryPassParams.geometryPassData.geometryBatches.size(); ++batchIdx)
	{
		const GeometryBatch& batch       = geometryPassParams.geometryPassData.geometryBatches[batchIdx];
		const BatchGPUData& batchGPUData = gpuBatches[batchIdx];

		lib::MTHandle<GeometryDrawMeshesDS> drawMeshesDS = graphBuilder.CreateDescriptorSet<GeometryDrawMeshesDS>(RENDERER_RESOURCE_NAME("GeometryDrawMeshesDS"));
		drawMeshesDS->u_drawCommands = batchGPUData.drawCommands;

		if constexpr (passIdx == EGeometryPass::VisibleGeometryPass)
		{
			drawMeshesDS->u_occludedMeshlets                = batchGPUData.occludedMeshlets;
			drawMeshesDS->u_occludedMeshletsCount           = batchGPUData.occludedMeshletsCount;
			drawMeshesDS->u_occludedMeshletsDispatchCommand = batchGPUData.dispatchOccludedMeshletsCommand;
		}

		const rdr::PipelineStateID pipeline = passContext.pipeline.CreatePipelineForBatch(geometryPassParams, batch, passIdx);

		const Uint32 maxDrawsCount = batch.batchElementsNum;

		IndirectGeometryBatchDrawParams indirectDrawParams;
		indirectDrawParams.drawCommands      = batchGPUData.drawCommands;
		indirectDrawParams.drawCommandsCount = batchGPUData.drawCommandsCount;

		graphBuilder.AddSubpass(RG_DEBUG_NAME_FORMATTED("Batch Subpass ({})", GeometryPassTraits<passIdx>::GetPassName()),
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


static void DrawDisoccludedMeshlets(rg::RenderGraphBuilder& graphBuilder, const GeometryPassParams& geometryPassParams, const GeometryPassContext& passContext, lib::Span<const BatchGPUData> gpuBatches)
{
	SPT_PROFILER_FUNCTION();

	constexpr EGeometryPass passIdx = EGeometryPass::DisoccludedMeshletsPass;

	CreateRenderPass<passIdx>(graphBuilder, geometryPassParams, passContext);

	for (SizeType batchIdx = 0; batchIdx < geometryPassParams.geometryPassData.geometryBatches.size(); ++batchIdx)
	{
		const GeometryBatch& batch       = geometryPassParams.geometryPassData.geometryBatches[batchIdx];
		const BatchGPUData& batchGPUData = gpuBatches[batchIdx];

		lib::MTHandle<GeometryDrawMeshes_DisoccludedMeshletsPassDS> drawMeshesDS = graphBuilder.CreateDescriptorSet<GeometryDrawMeshes_DisoccludedMeshletsPassDS>(RENDERER_RESOURCE_NAME("GeometryDrawMeshes_DisoccludedMeshletsPassDS"));
		drawMeshesDS->u_occludedMeshlets      = batchGPUData.occludedMeshlets;
		drawMeshesDS->u_occludedMeshletsCount = batchGPUData.occludedMeshletsCount;

		const rdr::PipelineStateID pipeline = passContext.pipeline.CreatePipelineForBatch(geometryPassParams, batch, passIdx);

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


static void DrawGeometryVisibleLastFrame(rg::RenderGraphBuilder& graphBuilder, const GeometryPassParams& geometryPassParams, const GeometryPassContext& passContext, lib::Span<const BatchGPUData> gpuBatches)
{
	SPT_PROFILER_FUNCTION();

	CullBatchElements<EGeometryPass::VisibleGeometryPass>(graphBuilder, geometryPassParams, gpuBatches);
	DrawBatchElements<EGeometryPass::VisibleGeometryPass>(graphBuilder, geometryPassParams, passContext, gpuBatches);
}


static void DrawDisoccludedGeometry(rg::RenderGraphBuilder& graphBuilder, const GeometryPassParams& geometryPassParams, const GeometryPassContext& passContext, lib::Span<const BatchGPUData> gpuBatches)
{
	SPT_PROFILER_FUNCTION();

	CullBatchElements<EGeometryPass::DisoccludedGeometryPass>(graphBuilder, geometryPassParams, gpuBatches);
	DrawBatchElements<EGeometryPass::DisoccludedGeometryPass>(graphBuilder, geometryPassParams, passContext, gpuBatches);
	DrawDisoccludedMeshlets(graphBuilder, geometryPassParams, passContext, gpuBatches);
}


static void RenderGeometryRenderPipeline(rg::RenderGraphBuilder& graphBuilder, const GeometryPassParams& geometryPassParams, const GeometryPassContext& passContext)
{
	SPT_PROFILER_FUNCTION();

	const lib::MTHandle<GeometryCullingDS> cullingDS = CreateCullingDS(geometryPassParams.hiZ, geometryPassParams.historyHiZ);

	const rg::BindDescriptorSetsScope geometryCullingDSScope(graphBuilder,
															 rg::BindDescriptorSets(cullingDS));

	passContext.pipeline.Prologue(graphBuilder, geometryPassParams);

	const lib::DynamicArray<BatchGPUData> gpuBatches = BuildGPUBatches(graphBuilder, geometryPassParams);

	DrawGeometryVisibleLastFrame(graphBuilder, geometryPassParams, passContext, gpuBatches);

	HiZ::CreateHierarchicalZ(graphBuilder, geometryPassParams.depth, geometryPassParams.hiZ->GetTexture());

	DrawDisoccludedGeometry(graphBuilder, geometryPassParams, passContext, gpuBatches);

	HiZ::CreateHierarchicalZ(graphBuilder, geometryPassParams.depth, geometryPassParams.hiZ->GetTexture());
}


void ExecutePipeline(rg::RenderGraphBuilder& graphBuilder, const GeometryPassParams& geometryPassParams, GeometryRenderingPipeline& pipeline)
{
	const GeometryPassContext passContext(pipeline);

	RenderGeometryRenderPipeline(graphBuilder, geometryPassParams, passContext);
}

} // spt::rsc::gp
