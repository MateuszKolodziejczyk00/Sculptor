#include "GeometryPipeline.h"
#include "ResourcesManager.h"
#include "RenderGraphBuilder.h"
#include "Utils/ViewRenderingSpec.h"
#include "SceneRenderer/RenderStages/Utils/hiZRenderer.h"


namespace spt::rsc::gp
{

struct GeometryPipelineContext
{
	explicit GeometryPipelineContext(GeometryRenderingPipeline& inPipeline)
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
	SHADER_STRUCT_FIELD(math::Vector2f,            hiZResolution)
	SHADER_STRUCT_FIELD(math::Vector2f,            historyHiZResolution)
	SHADER_STRUCT_FIELD(Bool,                      hasHistoryHiZ)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<Real32>, hiZTexture) // valid only for 2nd pass
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<Real32>, historyHiZTexture)
END_SHADER_STRUCT();


BEGIN_SHADER_STRUCT(GeometryCullSubmeshes_VisibleGeometryPassParams)
	SHADER_STRUCT_FIELD(gfx::RWTypedBuffer<GeometryDrawMeshTaskCommand>, drawCommands)
	SHADER_STRUCT_FIELD(gfx::RWTypedBuffer<Uint32>,                      drawCommandsCount)

	SHADER_STRUCT_FIELD(gfx::RWTypedBuffer<OccludedBatchElement>,                 occludedBatchElements)
	SHADER_STRUCT_FIELD(gfx::RWTypedBuffer<Uint32>,                               occludedBatchElementsCount)
	SHADER_STRUCT_FIELD(gfx::RWTypedBuffer<DispatchOccludedBatchElementsCommand>, dispatchOccludedElementsCommand)
END_SHADER_STRUCT();


BEGIN_SHADER_STRUCT(GeometryCullSubmeshes_DisoccludedGeometryPassParams)
	SHADER_STRUCT_FIELD(gfx::RWTypedBuffer<GeometryDrawMeshTaskCommand>, drawCommands)
	SHADER_STRUCT_FIELD(gfx::RWTypedBuffer<Uint32>,                      drawCommandsCount)

	SHADER_STRUCT_FIELD(gfx::TypedBuffer<OccludedBatchElement>, occludedBatchElements)
	SHADER_STRUCT_FIELD(gfx::TypedBuffer<Uint32>,               occludedBatchElementsCount)
END_SHADER_STRUCT();


BEGIN_RG_NODE_PARAMETERS_STRUCT(IndirectGeometryBatchDrawParams)
	RG_BUFFER_VIEW(drawCommands,      rg::ERGBufferAccess::Read, rhi::EPipelineStage::DrawIndirect)
	RG_BUFFER_VIEW(drawCommandsCount, rg::ERGBufferAccess::Read, rhi::EPipelineStage::DrawIndirect)
END_RG_NODE_PARAMETERS_STRUCT();


static rdr::GPUPtr<GeometryCullingParams> CreateCullingParams(rg::RenderGraphBuilder& graphBuilder, rg::RGTextureViewHandle hiZ, rg::RGTextureViewHandle historyHiZ)
{
	const Bool hasHistoryHiZ = historyHiZ.IsValid();

	GeometryCullingParams visCullingParams;
	visCullingParams.hiZResolution        = hiZ->GetResolution2D().cast<Real32>();
	visCullingParams.historyHiZResolution = hasHistoryHiZ ? historyHiZ->GetResolution2D().cast<Real32>() : math::Vector2f{};
	visCullingParams.hasHistoryHiZ        = hasHistoryHiZ;
	visCullingParams.hiZTexture        = hiZ;
	visCullingParams.historyHiZTexture = historyHiZ;

	return graphBuilder.CreateGPUData(visCullingParams);
}


BEGIN_SHADER_STRUCT(GeometryDrawMeshes_VisibleGeometryPassParams)
	SHADER_STRUCT_FIELD(gfx::TypedBuffer<GeometryDrawMeshTaskCommand>, drawCommands)
	
	SHADER_STRUCT_FIELD(gfx::RWTypedBuffer<OccludedMeshletData>,                  occludedMeshlets)
	SHADER_STRUCT_FIELD(gfx::RWTypedBuffer<Uint32>,                               occludedMeshletsCount)
	SHADER_STRUCT_FIELD(gfx::RWTypedBuffer<DispatchOccludedBatchElementsCommand>, occludedMeshletsDispatchCommand)
END_SHADER_STRUCT();


BEGIN_SHADER_STRUCT(GeometryDrawMeshes_DisoccludedGeometryPassParams)
	SHADER_STRUCT_FIELD(gfx::TypedBuffer<GeometryDrawMeshTaskCommand>, drawCommands)
END_SHADER_STRUCT();


BEGIN_SHADER_STRUCT(GeometryDrawMeshes_DisoccludedMeshletsPassParams)
	SHADER_STRUCT_FIELD(gfx::RWTypedBuffer<OccludedMeshletData>, occludedMeshlets)
	SHADER_STRUCT_FIELD(gfx::RWTypedBuffer<Uint32>,              occludedMeshletsCount)
END_SHADER_STRUCT();


template<EGeometryPass passIdx>
struct GeometryPassTraits
{
};

template<>
struct GeometryPassTraits<EGeometryPass::VisibleGeometryPass>
{
	using DrawMeshesParamsType = GeometryDrawMeshes_VisibleGeometryPassParams;

	static constexpr const char* GetPassName() { return "Visible Geometry Pass"; }
};

template<>
struct GeometryPassTraits<EGeometryPass::DisoccludedGeometryPass>
{
	using DrawMeshesParamsType = GeometryDrawMeshes_DisoccludedGeometryPassParams;

	static constexpr const char* GetPassName() { return "Disoccluded Geometry Pass"; }
};

template<>
struct GeometryPassTraits<EGeometryPass::DisoccludedMeshletsPass>
{
	using DrawMeshesParamsType = GeometryDrawMeshes_DisoccludedMeshletsPassParams;

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

static_assert(std::is_trivially_destructible_v<BatchGPUData>, "BatchGPUData must be trivially destructible");


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

	using CullSubmeshesParams = std::conditional_t<passIdx == EGeometryPass::VisibleGeometryPass, GeometryCullSubmeshes_VisibleGeometryPassParams, GeometryCullSubmeshes_DisoccludedGeometryPassParams>;

	for(SizeType batchIdx = 0u; batchIdx < geometryPassParams.geometryPassData.geometryBatches.size(); ++batchIdx)
	{
		const GeometryBatch& batch       = geometryPassParams.geometryPassData.geometryBatches[batchIdx];
		const BatchGPUData& batchGPUData = gpuBatches[batchIdx];

		graphBuilder.FillFullBuffer(RG_DEBUG_NAME("Initialize Draw Commands Count"), batchGPUData.drawCommandsCount, 0u);

		CullSubmeshesParams cullSubmeshesParams;
		cullSubmeshesParams.drawCommands               = batchGPUData.drawCommands;
		cullSubmeshesParams.drawCommandsCount          = batchGPUData.drawCommandsCount;
		cullSubmeshesParams.occludedBatchElements      = batchGPUData.occludedBatchElements;
		cullSubmeshesParams.occludedBatchElementsCount = batchGPUData.occludedBatchElementsCount;
		if constexpr (passIdx == EGeometryPass::VisibleGeometryPass)
		{
			cullSubmeshesParams.dispatchOccludedElementsCommand = batchGPUData.dispatchOccludedBatchElementsCommand;
		}

		if constexpr (passIdx == EGeometryPass::VisibleGeometryPass)
		{
			const Uint32 groupSize = 64u;
			const Uint32 dispatchGroups = math::Utils::DivideCeil(batch.batchElementsNum, groupSize);

			graphBuilder.Dispatch(RG_DEBUG_NAME_FORMATTED("Cull Submeshes ({})", GeometryPassTraits<passIdx>::GetPassName()),
								  CullBatchElementsPSO::passes[static_cast<Int32>(passIdx)],
								  dispatchGroups,
								  rg::ShaderParams(batch.batchData, cullSubmeshesParams));
		}
		else if constexpr (passIdx == EGeometryPass::DisoccludedGeometryPass)
		{
			graphBuilder.DispatchIndirect(RG_DEBUG_NAME_FORMATTED("Cull Submeshes ({})", GeometryPassTraits<passIdx>::GetPassName()),
										  CullBatchElementsPSO::passes[static_cast<Int32>(passIdx)],
										  batchGPUData.dispatchOccludedBatchElementsCommand,
										  0u,
										  rg::ShaderParams(batch.batchData, cullSubmeshesParams));
		}
	}
}


template<EGeometryPass passIdx>
static void CreateRenderPass(rg::RenderGraphBuilder& graphBuilder, const GeometryPassParams& geometryPassParams, const GeometryPipelineContext& pipelineContext)
{
	SPT_PROFILER_FUNCTION();

	const RenderPassDefinition renderPassDef = pipelineContext.pipeline.CreateRenderPassDefinition(graphBuilder, geometryPassParams, passIdx);
	
	graphBuilder.RenderPass(RG_DEBUG_NAME_FORMATTED("Geometry Pass ({})", GeometryPassTraits<passIdx>::GetPassName()),
							renderPassDef,
							rg::ShaderParams(renderPassDef.perPassParams),
							[resolution = renderPassDef.resolution](const lib::SharedRef<rdr::RenderContext>& renderContext, rdr::CommandRecorder& recorder)
							{
								recorder.SetViewport(math::AlignedBox2f(math::Vector2f(0.f, 0.f), resolution.cast<Real32>()), 0.f, 1.f);
								recorder.SetScissor(math::AlignedBox2u(math::Vector2u(0, 0), resolution));
							});
}


template<EGeometryPass passIdx>
static void DrawBatchElements(rg::RenderGraphBuilder& graphBuilder, const GeometryPassParams& geometryPassParams, const GeometryPipelineContext& pipelineContext, lib::Span<const BatchGPUData> gpuBatches)
{
	SPT_PROFILER_FUNCTION();

	static_assert(passIdx == EGeometryPass::VisibleGeometryPass || passIdx == EGeometryPass::DisoccludedGeometryPass);

	using GeometryDrawMeshesParams = typename GeometryPassTraits<passIdx>::DrawMeshesParamsType;

	CreateRenderPass<passIdx>(graphBuilder, geometryPassParams, pipelineContext);

	for (SizeType batchIdx = 0; batchIdx < geometryPassParams.geometryPassData.geometryBatches.size(); ++batchIdx)
	{
		const GeometryBatch& batch       = geometryPassParams.geometryPassData.geometryBatches[batchIdx];
		const BatchGPUData& batchGPUData = gpuBatches[batchIdx];

		GeometryDrawMeshesParams drawMeshesParams;
		drawMeshesParams.drawCommands = batchGPUData.drawCommands;

		if constexpr (passIdx == EGeometryPass::VisibleGeometryPass)
		{
			drawMeshesParams.occludedMeshlets                = batchGPUData.occludedMeshlets;
			drawMeshesParams.occludedMeshletsCount           = batchGPUData.occludedMeshletsCount;
			drawMeshesParams.occludedMeshletsDispatchCommand = batchGPUData.dispatchOccludedMeshletsCommand;
		}

		const rdr::PipelineStateID pipeline = pipelineContext.pipeline.CreatePipelineForBatch(geometryPassParams, batch, passIdx);

		const Uint32 maxDrawsCount = batch.batchElementsNum;

		IndirectGeometryBatchDrawParams indirectDrawParams;
		indirectDrawParams.drawCommands      = batchGPUData.drawCommands;
		indirectDrawParams.drawCommandsCount = batchGPUData.drawCommandsCount;

		graphBuilder.AddSubpass(RG_DEBUG_NAME_FORMATTED("Batch Subpass ({})", GeometryPassTraits<passIdx>::GetPassName()),
								rg::ShaderParams(drawMeshesParams, batch.batchData),
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


static void DrawDisoccludedMeshlets(rg::RenderGraphBuilder& graphBuilder, const GeometryPassParams& geometryPassParams, const GeometryPipelineContext& pipelineContext, lib::Span<const BatchGPUData> gpuBatches)
{
	SPT_PROFILER_FUNCTION();

	constexpr EGeometryPass passIdx = EGeometryPass::DisoccludedMeshletsPass;

	CreateRenderPass<passIdx>(graphBuilder, geometryPassParams, pipelineContext);

	for (SizeType batchIdx = 0; batchIdx < geometryPassParams.geometryPassData.geometryBatches.size(); ++batchIdx)
	{
		const GeometryBatch& batch       = geometryPassParams.geometryPassData.geometryBatches[batchIdx];
		const BatchGPUData& batchGPUData = gpuBatches[batchIdx];

		GeometryDrawMeshes_DisoccludedMeshletsPassParams drawMeshesParams;
		drawMeshesParams.occludedMeshlets      = batchGPUData.occludedMeshlets;
		drawMeshesParams.occludedMeshletsCount = batchGPUData.occludedMeshletsCount;

		const rdr::PipelineStateID pipeline = pipelineContext.pipeline.CreatePipelineForBatch(geometryPassParams, batch, passIdx);

		IndirectGeometryBatchDrawParams indirectDrawParams;
		indirectDrawParams.drawCommands = batchGPUData.dispatchOccludedMeshletsCommand;

		graphBuilder.AddSubpass(RG_DEBUG_NAME("Batch Subpass"),
								rg::ShaderParams(drawMeshesParams, batch.batchData),
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


static void DrawGeometryVisibleLastFrame(rg::RenderGraphBuilder& graphBuilder, const GeometryPassParams& geometryPassParams, const GeometryPipelineContext& pipelineContext, lib::Span<const BatchGPUData> gpuBatches)
{
	SPT_PROFILER_FUNCTION();

	CullBatchElements<EGeometryPass::VisibleGeometryPass>(graphBuilder, geometryPassParams, gpuBatches);
	DrawBatchElements<EGeometryPass::VisibleGeometryPass>(graphBuilder, geometryPassParams, pipelineContext, gpuBatches);
}


static void DrawDisoccludedGeometry(rg::RenderGraphBuilder& graphBuilder, const GeometryPassParams& geometryPassParams, const GeometryPipelineContext& pipelineContext, lib::Span<const BatchGPUData> gpuBatches)
{
	SPT_PROFILER_FUNCTION();

	CullBatchElements<EGeometryPass::DisoccludedGeometryPass>(graphBuilder, geometryPassParams, gpuBatches);
	DrawBatchElements<EGeometryPass::DisoccludedGeometryPass>(graphBuilder, geometryPassParams, pipelineContext, gpuBatches);
	DrawDisoccludedMeshlets(graphBuilder, geometryPassParams, pipelineContext, gpuBatches);
}


struct GeometryPipelineExecutor
{
	GeometryPipelineExecutor(rg::RenderGraphBuilder& graphBuilder, const GeometryPassParams& inGeometryPassParams, GeometryRenderingPipeline& inPipeline)
		 : geometryPassParams(inGeometryPassParams)
		 , pipelineContext(inPipeline)
	{
		cullingParams = CreateCullingParams(graphBuilder, geometryPassParams.hiZ, geometryPassParams.historyHiZ);

		gpuBatches = BuildGPUBatches(graphBuilder, geometryPassParams);
	}

	const GeometryPassParams& geometryPassParams;
	GeometryPipelineContext pipelineContext;

	rdr::GPUPtr<GeometryCullingParams> cullingParams;

	lib::DynamicArray<BatchGPUData> gpuBatches;
};


GeometryPipelineExecutor* CreateExecutor(rg::RenderGraphBuilder& graphBuilder, const GeometryPassParams& inGeometryPassParams, GeometryRenderingPipeline& inPipeline)
{
	return graphBuilder.GetMemoryArena().AllocateType<GeometryPipelineExecutor>(graphBuilder, inGeometryPassParams, inPipeline);
}


void DestroyExecutor(GeometryPipelineExecutor* executor)
{
	executor->~GeometryPipelineExecutor();
}


void ExecuteFirstPass(rg::RenderGraphBuilder& graphBuilder, const GeometryPipelineExecutor& executor)
{
	SPT_PROFILER_FUNCTION();

	const rg::BindShaderParamsScope geometryCullingParamsScope(graphBuilder, rg::ShaderParams(executor.cullingParams));

	executor.pipelineContext.pipeline.Prologue(graphBuilder, executor.geometryPassParams);

	DrawGeometryVisibleLastFrame(graphBuilder, executor.geometryPassParams, executor.pipelineContext, executor.gpuBatches);
}


void ExecuteSecondPass(rg::RenderGraphBuilder& graphBuilder, const GeometryPipelineExecutor& executor)
{
	SPT_PROFILER_FUNCTION();

	const rg::BindShaderParamsScope geometryCullingParamsScope(graphBuilder, rg::ShaderParams(executor.cullingParams));

	DrawDisoccludedGeometry(graphBuilder, executor.geometryPassParams, executor.pipelineContext, executor.gpuBatches);
}


void ExecutePipeline(rg::RenderGraphBuilder& graphBuilder, const GeometryPassParams& geometryPassParams, GeometryRenderingPipeline& pipeline)
{
	SPT_PROFILER_FUNCTION();

	GeometryPipelineExecutor executor(graphBuilder, geometryPassParams, pipeline);

	ExecuteFirstPass(graphBuilder, executor);

	HiZ::CreateHierarchicalZ(graphBuilder, geometryPassParams.depth, geometryPassParams.hiZ->GetTexture());

	ExecuteSecondPass(graphBuilder, executor);

	HiZ::CreateHierarchicalZ(graphBuilder, geometryPassParams.depth, geometryPassParams.hiZ->GetTexture());
}

} // spt::rsc::gp
