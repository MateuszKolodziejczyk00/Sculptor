#include "SRSpatiotemporalResampler.h"
#include "RenderGraphBuilder.h"
#include "Utils/ScreenSpaceTracer.h"
#include "View/RenderView.h"
#include "ShaderStructs/ShaderStructs.h"
#include "RenderGraphBuilder.h"
#include "SceneRenderer/Utils/BRDFIntegrationLUT.h"
#include "RenderScene.h"
#include "SceneRenderer/RenderStages/Utils/TracesAllocator.h"
#include "Pipelines/PSOsLibraryTypes.h"
#include "MaterialsSubsystem.h"
#include "Utils/ViewRenderingSpec.h"


namespace spt::rsc::sr_restir
{

namespace utils
{

class ReservoirsState
{
public:

	ReservoirsState(rg::RGBufferViewHandle bufferA, rg::RGBufferViewHandle bufferB)
		: m_inputReservoirsBuffer(bufferA)
		, m_outputReservoirsBuffer(bufferB)
		, m_swappedBuffers(false)
	{
	}

	rg::RGBufferViewHandle ReadReservoirs() const
	{
		return m_initialReservoirsBuffer.IsValid() ? m_initialReservoirsBuffer : m_inputReservoirsBuffer;
	}

	rg::RGBufferViewHandle WriteReservoirs()
	{
		return m_outputReservoirsBuffer;
	}

	void ReadNextFromInitialReservoirs(rg::RGBufferViewHandle initialReservoirs)
	{
		m_initialReservoirsBuffer = initialReservoirs;
	}

	void RollBuffers()
	{
		std::swap(m_inputReservoirsBuffer, m_outputReservoirsBuffer);
		m_swappedBuffers = !m_swappedBuffers;
		m_initialReservoirsBuffer = rg::RGBufferViewHandle();
	}

	Bool SwappedBuffers() const
	{
		return m_swappedBuffers;
	}

private:

	rg::RGBufferViewHandle m_inputReservoirsBuffer;
	rg::RGBufferViewHandle m_outputReservoirsBuffer;

	rg::RGBufferViewHandle m_initialReservoirsBuffer;

	Bool m_swappedBuffers;
};

} // utils

BEGIN_SHADER_STRUCT(SRResamplingConstants)
	SHADER_STRUCT_FIELD(math::Vector2u, resolution)
	SHADER_STRUCT_FIELD(math::Vector2u, reservoirsResolution)
	SHADER_STRUCT_FIELD(math::Vector2f, pixelSize)
	SHADER_STRUCT_FIELD(Uint32,         frameIdx)
	SHADER_STRUCT_FIELD(Real32,         resamplingRangeStep)
	SHADER_STRUCT_FIELD(SSTracerData,   ssTracer)
	SHADER_STRUCT_FIELD(Real32,         ssrTraceLength)
END_SHADER_STRUCT();


static SRResamplingConstants CreateResamplingConstants(const ResamplingParams& params)
{
	const math::Vector2u resolution = params.GetResolution();

	SRResamplingConstants resamplingConstants;
	resamplingConstants.resolution           = resolution;
	resamplingConstants.reservoirsResolution = ComputeReservoirsResolution(resolution);
	resamplingConstants.pixelSize            = resolution.cast<Real32>().cwiseInverse();
	resamplingConstants.frameIdx             = params.viewSpec.GetFrameIdx();
	resamplingConstants.resamplingRangeStep  = params.resamplingRangeStep;
	resamplingConstants.ssTracer             = CreateScreenSpaceTracerData(params.viewSpec.GetShadingViewContext().linearDepth, params.ssrStepsNum);
	resamplingConstants.ssrTraceLength       = params.ssrTraceLength;

	return resamplingConstants;
}


namespace copy
{

BEGIN_SHADER_STRUCT(RTCopyTracedReservoirsParams)
	SHADER_STRUCT_FIELD(gfx::RWTypedBufferRef<SRPackedReservoir>,         outReservoirs)
	SHADER_STRUCT_FIELD(gfx::TypedBufferRef<SRPackedReservoir>,           inReservoirs)
	SHADER_STRUCT_FIELD(gfx::TypedBufferRef<vrt::EncodedRayTraceCommand>, traceCommands)
	SHADER_STRUCT_FIELD(gfx::TypedBufferRef<Uint32>,                      tracesNum)
	SHADER_STRUCT_FIELD(rdr::GPUPtr<SRResamplingConstants>,               resamplingConstants)
END_SHADER_STRUCT();


COMPUTE_PSO(CopyTracedReservoirsPSO)
{
	COMPUTE_SHADER("Sculptor/SpecularReflections/RTCopyTracedReservoirs.hlsl", RTCopyTracedReservoirsCS);

	PRESET(pso);

	static void PrecachePSOs(rdr::PSOCompilerInterface& compiler, const rdr::PSOPrecacheParams& params)
	{
		pso = CompilePSO(compiler, { });
	}
};


static void CopyTracedReservoirs(rg::RenderGraphBuilder& graphBuilder, const rdr::GPUPtr<SRResamplingConstants>& resamplingConstants, const vrt::TracesAllocation& traces, rg::RGBufferViewHandle inputBuffer, rg::RGBufferViewHandle outputBuffer)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(inputBuffer.IsValid());
	SPT_CHECK(outputBuffer.IsValid());
	SPT_CHECK(inputBuffer != outputBuffer);

	RTCopyTracedReservoirsParams shaderConstants;
	shaderConstants.outReservoirs       = outputBuffer;
	shaderConstants.inReservoirs        = inputBuffer;
	shaderConstants.traceCommands       = traces.rayTraceCommands;
	shaderConstants.tracesNum           = traces.tracesNum;
	shaderConstants.resamplingConstants = resamplingConstants;

	graphBuilder.DispatchIndirect(RG_DEBUG_NAME("Copy Traced Reservoirs"),
								  CopyTracedReservoirsPSO::pso,
								  traces.dispatchIndirectArgs, 0u,
								  rg::ShaderParams(shaderConstants));
}

} // copy


namespace temporal
{

BEGIN_SHADER_STRUCT(TemporalResamplingConstants)
	SHADER_STRUCT_FIELD(Uint32,                                   variableRateTileSizeBitOffset)
	SHADER_STRUCT_FIELD(Uint32,                                   enableHitDistanceBasedMaxAge)
	SHADER_STRUCT_FIELD(Uint32,                                   reservoirMaxAge)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2DRef<Real32>,             depthTexture)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2DRef<math::Vector2f>,     normalsTexture)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2DRef<Real32>,             roughnessTexture)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2DRef<math::Vector4f>,     baseColorTexture)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2DRef<math::Vector2f>,     motionTexture)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2DRef<Real32>,             historyDepthTexture)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2DRef<math::Vector2f>,     historyNormalsTexture)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2DRef<Real32>,             historyRoughnessTexture)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2DRef<math::Vector4f>,     historyBaseColorTexture)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<Real32>,                historySpecularHitDist)
	SHADER_STRUCT_FIELD(gfx::RWTypedBufferRef<SRPackedReservoir>, initialResservoirsBuffer)
	SHADER_STRUCT_FIELD(gfx::TypedBufferRef<SRPackedReservoir>,   historyReservoirsBuffer)
	SHADER_STRUCT_FIELD(gfx::RWTypedBufferRef<SRPackedReservoir>, outReservoirsBuffer)
	SHADER_STRUCT_FIELD(gfx::UAVTexture2DRef<Uint32>,             rwVariableRateBlocksTexture)
	SHADER_STRUCT_FIELD(rdr::GPUPtr<SRResamplingConstants>,       resamplingConstants)
END_SHADER_STRUCT();


BEGIN_SHADER_STRUCT(SRAdditionalPassesAllocatorParams)
	SHADER_STRUCT_FIELD(gfx::RWTypedBufferRef<vrt::EncodedRayTraceCommand>, rayTracesCommands)
	SHADER_STRUCT_FIELD(gfx::RWTypedBufferRef<Uint32>,                      commandsNum)
	SHADER_STRUCT_FIELD(gfx::RWTypedBufferRef<Uint32>,                      tracesDispatchGroupsNum)
	SHADER_STRUCT_FIELD(gfx::RWTypedBufferRef<Uint32>,                      tracesNum)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2DRef<Uint32>,                       vrReprojectionSuccessMask)
END_SHADER_STRUCT();


COMPUTE_PSO(ResampleTemporallyPSO)
{
	COMPUTE_SHADER("Sculptor/SpecularReflections/ResampleTemporally.hlsl", ResampleTemporallyCS);

	PRESET(withSecondTracingPass);
	PRESET(noSecondTracingPass);

	static void PrecachePSOs(rdr::PSOCompilerInterface& compiler, const rdr::PSOPrecacheParams& params)
	{
		withSecondTracingPass = CompilePSO(compiler, { "ENABLE_SECOND_TRACING_PASS=1" });
		noSecondTracingPass = CompilePSO(compiler, { "ENABLE_SECOND_TRACING_PASS=0" });
	}
};


static vrt::TracesAllocation PrepareAdditionalTracesAllocationData(rg::RenderGraphBuilder& graphBuilder, const ResamplingParams& params)
{
	SPT_PROFILER_FUNCTION();

	const vrt::TracesAllocation& primaryTracesAllocaiton = params.tracesAllocation;

	vrt::TracesAllocation additionalTracesAllocation;
	additionalTracesAllocation.rayTraceCommands          = graphBuilder.CreateBufferView(RG_DEBUG_NAME("Additional Ray Trace Commands Buffer"), primaryTracesAllocaiton.rayTraceCommands->GetBufferDefinition(), rhi::EMemoryUsage::GPUOnly);
	additionalTracesAllocation.tracingIndirectArgs       = graphBuilder.CreateBufferView(RG_DEBUG_NAME("Additional Tracing Indirect Args Buffer"), primaryTracesAllocaiton.tracingIndirectArgs->GetBufferDefinition(), rhi::EMemoryUsage::GPUOnly);
	additionalTracesAllocation.variableRateBlocksTexture = primaryTracesAllocaiton.variableRateBlocksTexture;
	additionalTracesAllocation.dispatchIndirectArgs      = graphBuilder.CreateBufferView(RG_DEBUG_NAME("Additional Dispatch Indirect Args Buffer"), primaryTracesAllocaiton.dispatchIndirectArgs->GetBufferDefinition(), rhi::EMemoryUsage::GPUOnly);
	additionalTracesAllocation.tracesNum                 = graphBuilder.CreateBufferView(RG_DEBUG_NAME("Additional Traces Num Buffer"), primaryTracesAllocaiton.tracesNum->GetBufferDefinition(), rhi::EMemoryUsage::GPUOnly);

	graphBuilder.FillFullBuffer(RG_DEBUG_NAME("Clear Additional Traces Num"), additionalTracesAllocation.tracesNum, 0u);
	graphBuilder.FillFullBuffer(RG_DEBUG_NAME("Clear Additional Tracing Indirect Args"), additionalTracesAllocation.tracingIndirectArgs, 0u);
	graphBuilder.FillFullBuffer(RG_DEBUG_NAME("Clear Additional Dispatch Indirect Args"), additionalTracesAllocation.dispatchIndirectArgs, 1u);

	return additionalTracesAllocation;
}


static vrt::TracesAllocation ResampleTemporally(rg::RenderGraphBuilder& graphBuilder, const ResamplingParams& params, const rdr::GPUPtr<SRResamplingConstants>& resamplingConstants, utils::ReservoirsState& reservoirsState)
{
	SPT_PROFILER_FUNCTION();

	const math::Vector2u resolution = params.GetResolution();

	SPT_CHECK(!params.enableHitDistanceBasedMaxAge || params.historySpecularHitDist.IsValid());
	SPT_CHECK(!params.enableHitDistanceBasedMaxAge || params.historySpecularHitDist->GetResolution2D() == resolution);

	TemporalResamplingConstants passConstants;
	passConstants.variableRateTileSizeBitOffset = params.variableRateTileSizeBitOffset;
	passConstants.enableHitDistanceBasedMaxAge  = params.enableHitDistanceBasedMaxAge;
	passConstants.reservoirMaxAge               = params.reservoirMaxAge;
	passConstants.depthTexture                = params.depthTexture;
	passConstants.normalsTexture              = params.normalsTexture;
	passConstants.roughnessTexture            = params.roughnessTexture;
	passConstants.baseColorTexture            = params.baseColorTexture;
	passConstants.motionTexture               = params.motionTexture;
	passConstants.historyDepthTexture         = params.historyDepthTexture;
	passConstants.historyNormalsTexture       = params.historyNormalsTexture;
	passConstants.historyRoughnessTexture     = params.historyRoughnessTexture;
	passConstants.historyBaseColorTexture     = params.historyBaseColorTexture;
	passConstants.historySpecularHitDist      = params.historySpecularHitDist;
	passConstants.initialResservoirsBuffer    = params.initialReservoirBuffer;
	passConstants.historyReservoirsBuffer     = reservoirsState.ReadReservoirs();
	passConstants.outReservoirsBuffer         = reservoirsState.WriteReservoirs();
	passConstants.rwVariableRateBlocksTexture = params.tracesAllocation.variableRateBlocksTexture;
	passConstants.resamplingConstants         = resamplingConstants;

	vrt::TracesAllocation additionalTracesAllocation;
	rdr::GPUPtr<SRAdditionalPassesAllocatorParams> additionalPassesAllocatorConstants;
	if (params.enableSecondTracingPass)
	{
		additionalTracesAllocation = PrepareAdditionalTracesAllocationData(graphBuilder, params);

		SRAdditionalPassesAllocatorParams allocatorConstants;
		allocatorConstants.rayTracesCommands         = additionalTracesAllocation.rayTraceCommands;
		allocatorConstants.commandsNum               = additionalTracesAllocation.tracingIndirectArgs;
		allocatorConstants.tracesDispatchGroupsNum   = additionalTracesAllocation.dispatchIndirectArgs;
		allocatorConstants.tracesNum                 = additionalTracesAllocation.tracesNum;
		allocatorConstants.vrReprojectionSuccessMask = params.vrReprojectionSuccessMask;

		additionalPassesAllocatorConstants = graphBuilder.CreateGPUData(allocatorConstants);
	}

	reservoirsState.RollBuffers();

	graphBuilder.Dispatch(RG_DEBUG_NAME("Resample Temporally"),
						  params.enableSecondTracingPass ? ResampleTemporallyPSO::withSecondTracingPass : ResampleTemporallyPSO::noSecondTracingPass,
						  math::Utils::DivideCeil(resolution, math::Vector2u(8u, 8u)),
						  rg::ShaderParams(passConstants, additionalPassesAllocatorConstants));

	return additionalTracesAllocation;
}

} // temporal

namespace firefly_filter
{

BEGIN_SHADER_STRUCT(RTFireflyFilterParams)
	SHADER_STRUCT_FIELD(gfx::RWTypedBufferRef<SRPackedReservoir>, inOutReservoirsBuffer)
	SHADER_STRUCT_FIELD(rdr::GPUPtr<SRResamplingConstants>, resamplingConstants)
END_SHADER_STRUCT();


COMPUTE_PSO(RTFireflyFilterPSO)
{
	COMPUTE_SHADER("Sculptor/SpecularReflections/RTFireflyFilter.hlsl", RTFireflyFilterCS);

	PRESET(pso);

	static void PrecachePSOs(rdr::PSOCompilerInterface& compiler, const rdr::PSOPrecacheParams& params)
	{
		pso = CompilePSO(compiler, { });
	}
};


static void FireflyFilter(rg::RenderGraphBuilder& graphBuilder, const ResamplingParams& params, const rdr::GPUPtr<SRResamplingConstants>& resamplingConstants, utils::ReservoirsState& reservoirsState)
{
	SPT_PROFILER_FUNCTION();

	const math::Vector2u resolution = params.GetResolution();

	RTFireflyFilterParams shaderConstants;
	shaderConstants.inOutReservoirsBuffer = reservoirsState.ReadReservoirs();
	shaderConstants.resamplingConstants   = resamplingConstants;

	graphBuilder.Dispatch(RG_DEBUG_NAME("RT Firefly Filter"),
						  RTFireflyFilterPSO::pso,
						  math::Utils::DivideCeil(resolution, math::Vector2u(16u, 16u)),
						  rg::ShaderParams(shaderConstants));
}

} // firefly_filter

namespace spatial
{

BEGIN_SHADER_STRUCT(SpatialResamplingPassConstants)
	SHADER_STRUCT_FIELD(Uint32,                                   seed)
	SHADER_STRUCT_FIELD(Real32,                                   resamplingRangeMultiplier)
	SHADER_STRUCT_FIELD(Uint32,                                   resampleOnlyFromTracedPixels)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2DRef<Real32>,             depthTexture)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2DRef<math::Vector2f>,     normalsTexture)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2DRef<Real32>,             roughnessTexture)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2DRef<Uint32>,             variableRateBlocksTexture)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2DRef<math::Vector4f>,     baseColorTexture)
	SHADER_STRUCT_FIELD(gfx::TypedBufferRef<SRPackedReservoir>,   inReservoirsBuffer)
	SHADER_STRUCT_FIELD(gfx::RWTypedBufferRef<SRPackedReservoir>, outReservoirsBuffer)
	SHADER_STRUCT_FIELD(rdr::GPUPtr<SRResamplingConstants>,       resamplingConstants)
END_SHADER_STRUCT();


static rdr::PipelineStateID CompileResampleSpatiallyPipeline(Uint32 samplesNum, Bool enableScreenSpaceVisibilityTrace)
{
	sc::ShaderCompilationSettings compilationSettings;
	compilationSettings.AddMacroDefinition(sc::MacroDefinition("SPATIAL_RESAMPLING_SAMPLES_NUM", std::to_string(samplesNum).c_str()));
	compilationSettings.AddMacroDefinition(sc::MacroDefinition("SPATIAL_RESAMPLING_ENABLE_SS_VISIBILITY", enableScreenSpaceVisibilityTrace ? "1" : "0"));

	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/SpecularReflections/ResampleSpatially.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "ResampleSpatiallyCS"), compilationSettings);

	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("Resample Spatially Pipeline"), shader);
}


static void ResampleSpatially(rg::RenderGraphBuilder& graphBuilder, const ResamplingParams& params, const rdr::GPUPtr<SRResamplingConstants>& resamplingConstants, const SpatialResamplingPassParams& passParams, utils::ReservoirsState& reservoirsState)
{
	SPT_PROFILER_FUNCTION();

	const math::Vector2u resolution = params.GetResolution();

	SpatialResamplingPassConstants passConstants;
	passConstants.seed                         = lib::rnd::RandomFromTypeDomain<Uint32>();
	passConstants.resamplingRangeMultiplier    = passParams.resamplingRangeMultiplier;
	passConstants.resampleOnlyFromTracedPixels = passParams.resampleOnlyFromTracedPixels;
	passConstants.depthTexture              = params.depthTexture;
	passConstants.normalsTexture            = params.normalsTexture;
	passConstants.roughnessTexture          = params.roughnessTexture;
	passConstants.baseColorTexture          = params.baseColorTexture;
	passConstants.variableRateBlocksTexture = params.tracesAllocation.variableRateBlocksTexture;
	passConstants.inReservoirsBuffer        = reservoirsState.ReadReservoirs();
	passConstants.outReservoirsBuffer       = reservoirsState.WriteReservoirs();
	passConstants.resamplingConstants       = resamplingConstants;

	reservoirsState.RollBuffers();

	const rdr::PipelineStateID pipeline = CompileResampleSpatiallyPipeline(passParams.samplesNum, passParams.enableScreenSpaceVisibilityTrace);

	graphBuilder.Dispatch(RG_DEBUG_NAME("Resample Spatially"),
						  pipeline,
						  math::Utils::DivideCeil(resolution, math::Vector2u(8u, 8u)),
						  rg::ShaderParams(passConstants));

}

} // spatial

namespace final_visibility
{

BEGIN_SHADER_STRUCT(SRResamplingFinalVisibilityTestParams)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2DRef<Real32>,                     depthTexture)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2DRef<math::Vector2f>,             normalsTexture)
	SHADER_STRUCT_FIELD(gfx::TypedBufferRef<vrt::EncodedRayTraceCommand>, traceCommands)
	SHADER_STRUCT_FIELD(gfx::RWTypedBufferRef<SRPackedReservoir>,         inOutReservoirsBuffer)
	SHADER_STRUCT_FIELD(gfx::TypedBufferRef<SRPackedReservoir>,           initialReservoirsBuffer)
	SHADER_STRUCT_FIELD(rdr::GPUPtr<SRResamplingConstants>,               resamplingConstants)
END_SHADER_STRUCT();


RT_PSO(SRFinalVisibilityTestPSO)
{
	RAY_GEN_SHADER("Sculptor/SpecularReflections/ResamplingFinalVisibility.hlsl", ResamplingFinalVisibilityTestRTG);
	MISS_SHADERS(SHADER_ENTRY("Sculptor/SpecularReflections/ResamplingFinalVisibility.hlsl", GenericRTM));

	HIT_GROUP
	{
		ANY_HIT_SHADER("Sculptor/SpecularReflections/ResamplingFinalVisibility.hlsl", GenericAH);

		HIT_PERMUTATION_DOMAIN(mat::RTHitGroupPermutation);
	};

	PRESET(fullRate);
	PRESET(variableRate);

	static void PrecachePSOs(rdr::PSOCompilerInterface& compiler, const rdr::PSOPrecacheParams& params)
	{
		const lib::DynamicArray<HitGroup> hitGroups = mat::MaterialsSubsystem::Get().GetRTHitGroups<HitGroup>();
		const rhi::RayTracingPipelineDefinition psoDefinition{ .maxRayRecursionDepth = 1u };
		fullRate     = CompilePSO(compiler, psoDefinition, hitGroups, { "FORCE_FULL_RATE=1" });
		variableRate = CompilePSO(compiler, psoDefinition, hitGroups, { "FORCE_FULL_RATE=0" });
	}
};


static void ExecuteFinalVisibilityTest(rg::RenderGraphBuilder& graphBuilder, const ResamplingParams& params, const rdr::GPUPtr<SRResamplingConstants>& resamplingConstants, utils::ReservoirsState& reservoirsState)
{
	SPT_PROFILER_FUNCTION();

	const math::Vector2u resolution = params.GetResolution();

	SRResamplingFinalVisibilityTestParams shaderConstants;
	shaderConstants.depthTexture            = params.depthTexture;
	shaderConstants.normalsTexture          = params.normalsTexture;
	shaderConstants.traceCommands           = params.tracesAllocation.rayTraceCommands;
	shaderConstants.inOutReservoirsBuffer   = reservoirsState.ReadReservoirs();
	shaderConstants.initialReservoirsBuffer = params.initialReservoirBuffer;
	shaderConstants.resamplingConstants     = resamplingConstants;

	if (params.doFullFinalVisibilityCheck)
	{
		graphBuilder.TraceRays(RG_DEBUG_NAME("SR Final Visibility Test"),
									   SRFinalVisibilityTestPSO::fullRate,
									   resolution,
									   rg::ShaderParams(shaderConstants));
	}
	else
	{
		graphBuilder.TraceRaysIndirect(RG_DEBUG_NAME("SR Final Visibility Test"),
									   SRFinalVisibilityTestPSO::variableRate,
									   params.tracesAllocation.tracingIndirectArgs, 0,
									   rg::ShaderParams(shaderConstants));
	}
}

} // final_visibility

namespace resolve
{

BEGIN_SHADER_STRUCT(ResolveReservoirsParams)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2DRef<Real32>,           depthTexture)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2DRef<math::Vector2f>,   normalsTexture)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2DRef<Real32>,           roughnessTexture)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2DRef<math::Vector4f>,   baseColorTexture)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2DRef<Uint32>,           variableRateBlocksTexture)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2DRef<math::Vector2f>,   brdfIntegrationLUT)
	SHADER_STRUCT_FIELD(gfx::UAVTexture2DRef<math::Vector4f>,   specularLumHitDistanceTexture)
	SHADER_STRUCT_FIELD(gfx::UAVTexture2DRef<math::Vector4f>,   diffuseLumHitDistanceTexture)
	SHADER_STRUCT_FIELD(gfx::UAVTexture2DRef<math::Vector2f>,   lightDirectionTexture)
	SHADER_STRUCT_FIELD(gfx::TypedBufferRef<SRPackedReservoir>, reservoirsBuffer)
	SHADER_STRUCT_FIELD(gfx::TypedBufferRef<SRPackedReservoir>, initialReservoirsBuffer)
	SHADER_STRUCT_FIELD(rdr::GPUPtr<SRResamplingConstants>,     resamplingConstants)
END_SHADER_STRUCT();


SIMPLE_COMPUTE_PSO(ResolveReservoirsPSO, "Sculptor/SpecularReflections/ResolveReservoirs.hlsl", ResolveReservoirsCS);


static void ResolveReservoirs(rg::RenderGraphBuilder& graphBuilder, const ResamplingParams& params, const rdr::GPUPtr<SRResamplingConstants>& resamplingConstants, utils::ReservoirsState& reservoirsState)
{
	SPT_PROFILER_FUNCTION();

	const math::Vector2u resolution = params.GetResolution();

	ResolveReservoirsParams shaderConstants;
	shaderConstants.depthTexture                  = params.depthTexture;
	shaderConstants.normalsTexture                = params.normalsTexture;
	shaderConstants.roughnessTexture              = params.roughnessTexture;
	shaderConstants.baseColorTexture              = params.baseColorTexture;
	shaderConstants.variableRateBlocksTexture     = params.tracesAllocation.variableRateBlocksTexture;
	shaderConstants.brdfIntegrationLUT            = BRDFIntegrationLUT::Get().GetLUT(graphBuilder);
	shaderConstants.specularLumHitDistanceTexture = params.outSpecularLuminanceDistTexture;
	shaderConstants.diffuseLumHitDistanceTexture  = params.outDiffuseLuminanceDistTexture;
	shaderConstants.lightDirectionTexture         = params.outLightDirectionTexture;
	shaderConstants.reservoirsBuffer              = reservoirsState.ReadReservoirs();
	shaderConstants.initialReservoirsBuffer       = params.initialReservoirBuffer;
	shaderConstants.resamplingConstants           = resamplingConstants;

	graphBuilder.Dispatch(RG_DEBUG_NAME("Resolve Reservoirs"),
						  ResolveReservoirsPSO::pso,
						  math::Utils::DivideCeil(resolution, math::Vector2u(8u, 8u)),
						  rg::ShaderParams(shaderConstants));
}

} // resolve

//////////////////////////////////////////////////////////////////////////////////////////////////
// Helpers =======================================================================================

Uint64 ComputeReservoirsBufferSize(math::Vector2u resolution)
{
	return resolution.x() * resolution.y() * sizeof(SRPackedReservoir);
}

math::Vector2u ComputeReservoirsResolution(math::Vector2u resolution)
{
	return math::Vector2u(math::Utils::RoundUp(resolution.x(), 64u), math::Utils::RoundUp(resolution.y(), 64u));
}

rg::RGBufferViewHandle CreateReservoirsBuffer(rg::RenderGraphBuilder& graphBuilder, math::Vector2u resolution)
{
	SPT_PROFILER_FUNCTION();

	const Uint64 bufferSize = ComputeReservoirsBufferSize(resolution);

	rhi::BufferDefinition bufferDef;
	bufferDef.size  = bufferSize;
	bufferDef.usage = lib::Flags(rhi::EBufferUsage::Storage, rhi::EBufferUsage::TransferDst);

	return graphBuilder.CreateBufferView(RG_DEBUG_NAME("Reservoir Buffer"), bufferDef, rhi::EMemoryUsage::GPUOnly);
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// ResamplingParams ==============================================================================

ResamplingParams::ResamplingParams(const ViewRenderingSpec& inViewSpec, const RenderScene& inRenderScene)
	: viewSpec(inViewSpec)
	, renderScene(inRenderScene)
{
}

math::Vector2u ResamplingParams::GetResolution() const
{
	return outSpecularLuminanceDistTexture->GetResolution2D();
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// SpatiotemporalResampler =======================================================================

SpatiotemporalResampler::SpatiotemporalResampler()
	: m_historyResolution(0u, 0u)
{ }

InitialResamplingResult SpatiotemporalResampler::ExecuteInitialResampling(rg::RenderGraphBuilder& graphBuilder, const ResamplingParams& params)
{
	SPT_PROFILER_FUNCTION();

	SPT_RG_DIAGNOSTICS_SCOPE(graphBuilder, "RESTIR: Initial Resampling");

	InitialResamplingResult result;

	PrepareForResampling(graphBuilder, params);

	const rg::RGBufferViewHandle historyReservoirsBuffer = graphBuilder.AcquireExternalBufferView(m_inputTemporalReservoirBuffer->GetFullView());
	const rg::RGBufferViewHandle outputReservoirsBuffer  = graphBuilder.AcquireExternalBufferView(m_outputTemporalReservoirBuffer->GetFullView());

	const rdr::GPUPtr<SRResamplingConstants> resamplingConstants = graphBuilder.CreateGPUData(CreateResamplingConstants(params));

	utils::ReservoirsState reservoirsState(historyReservoirsBuffer, outputReservoirsBuffer);
	if (params.enableTemporalResampling && HasValidTemporalData(params))
	{
		SPT_CHECK(params.historyDepthTexture.IsValid());
		SPT_CHECK(params.historyRoughnessTexture.IsValid());
		SPT_CHECK(params.historyBaseColorTexture.IsValid());
		SPT_CHECK(params.historyNormalsTexture.IsValid());

		result.additionalTracesAllocation = temporal::ResampleTemporally(graphBuilder, params, resamplingConstants, reservoirsState);
	
		result.resampledReservoirsBuffer = reservoirsState.ReadReservoirs();
	}
	else
	{
		result.resampledReservoirsBuffer = params.initialReservoirBuffer;
	}

	return result;
}

void SpatiotemporalResampler::ExecuteFinalResampling(rg::RenderGraphBuilder& graphBuilder, const ResamplingParams& params, const InitialResamplingResult& initialResamplingResult)
{
	SPT_PROFILER_FUNCTION();

	SPT_RG_DIAGNOSTICS_SCOPE(graphBuilder, "RESTIR: Final Resampling");

	const math::Vector2u resolution = params.GetResolution();

	const rg::RGBufferViewHandle historyReservoirsBuffer = graphBuilder.AcquireExternalBufferView(m_inputTemporalReservoirBuffer->GetFullView());
	const rg::RGBufferViewHandle outputReservoirsBuffer  = graphBuilder.AcquireExternalBufferView(m_outputTemporalReservoirBuffer->GetFullView());

	const rdr::GPUPtr<SRResamplingConstants> resamplingConstants = graphBuilder.CreateGPUData(CreateResamplingConstants(params));

	utils::ReservoirsState reservoirsState(historyReservoirsBuffer, outputReservoirsBuffer);

	if (initialResamplingResult.resampledReservoirsBuffer->GetBuffer() == outputReservoirsBuffer->GetBuffer())
	{
		// roll buffers if current reservoirs were already written to output buffer
		reservoirsState.RollBuffers();
	}
	else if(initialResamplingResult.resampledReservoirsBuffer->GetBuffer() == params.initialReservoirBuffer->GetBuffer())
	{
		// if initial phase was skipped, we need to read initial reservoirs
		reservoirsState.ReadNextFromInitialReservoirs(params.initialReservoirBuffer);
	}

	// Copy additional traces to the "initial" buffer
	if (initialResamplingResult.additionalTracesAllocation.IsValid())
	{
		copy::CopyTracedReservoirs(graphBuilder, resamplingConstants, initialResamplingResult.additionalTracesAllocation, reservoirsState.ReadReservoirs(), params.initialReservoirBuffer);
	}

	if(params.enableFireflyFilter)
	{
		firefly_filter::FireflyFilter(graphBuilder, params, resamplingConstants, reservoirsState);
	}

	for (const SpatialResamplingPassParams& spatialPassParams : params.spatialResamplingPasses)
	{
		spatial::ResampleSpatially(graphBuilder, params, resamplingConstants, spatialPassParams, reservoirsState);
	}

	final_visibility::ExecuteFinalVisibilityTest(graphBuilder, params, resamplingConstants, reservoirsState);

	resolve::ResolveReservoirs(graphBuilder, params, resamplingConstants, reservoirsState);

	if (params.enableTemporalResampling)
	{
		m_historyResolution = resolution;
		if (reservoirsState.SwappedBuffers())
		{
			std::swap(m_inputTemporalReservoirBuffer, m_outputTemporalReservoirBuffer);
		}
	}
	else
	{
		m_historyResolution = math::Vector2u(0u, 0u);
	}
}

Bool SpatiotemporalResampler::HasValidTemporalData(const ResamplingParams& params) const
{
	return !!m_inputTemporalReservoirBuffer && m_historyResolution == params.GetResolution();
}

void SpatiotemporalResampler::PrepareForResampling(rg::RenderGraphBuilder& graphBuilder, const ResamplingParams& params)
{
	SPT_PROFILER_FUNCTION();

	const math::Vector2u resolution = params.GetResolution();

	if (m_historyResolution != resolution)
	{
		const math::Vector2u reservoirsResolution = ComputeReservoirsResolution(resolution);

		m_inputTemporalReservoirBuffer  = CreateTemporalReservoirBuffer(graphBuilder, reservoirsResolution);
		m_outputTemporalReservoirBuffer = CreateTemporalReservoirBuffer(graphBuilder, reservoirsResolution);
	}
}

lib::SharedPtr<rdr::Buffer> SpatiotemporalResampler::CreateTemporalReservoirBuffer(rg::RenderGraphBuilder& graphBuilder, const math::Vector2u& resolution) const
{
	SPT_PROFILER_FUNCTION();

	const Uint64 bufferSize = ComputeReservoirsBufferSize(resolution);

	rhi::BufferDefinition bufferDef;
	bufferDef.size  = bufferSize;
	bufferDef.usage = rhi::EBufferUsage::Storage;

	return rdr::ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME("Temporal Reservoir Buffer"), bufferDef, rhi::EMemoryUsage::GPUOnly);
}

} // spt::rsc::sr_restir
