#include "SRSpatiotemporalResampler.h"
#include "RenderGraphBuilder.h"
#include "View/RenderView.h"
#include "ShaderStructs/ShaderStructsMacros.h"
#include "RGDescriptorSetState.h"
#include "DescriptorSetBindings/ConstantBufferBinding.h"
#include "DescriptorSetBindings/SRVTextureBinding.h"
#include "DescriptorSetBindings/RWTextureBinding.h"
#include "DescriptorSetBindings/RWBufferBinding.h"
#include "DescriptorSetBindings/SamplerBinding.h"
#include "RenderGraphBuilder.h"
#include "SceneRenderer/Utils/BRDFIntegrationLUT.h"
#include "RayTracing/RayTracingRenderSceneSubsystem.h"
#include "RenderScene.h"
#include "SceneRenderer/Utils/RTVisibilityUtils.h"
#include "MaterialsUnifiedData.h"
#include "SceneRenderer/RenderStages/Utils/TracesAllocator.h"


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
END_SHADER_STRUCT();


static SRResamplingConstants CreateResamplingConstants(const ResamplingParams& params)
{
	const math::Vector2u resolution = params.GetResolution();

	const Real32 resMinAxis = static_cast<Real32>(std::min(resolution.x(), resolution.y()));

	SRResamplingConstants resamplingConstants;
	resamplingConstants.resolution           = resolution;
	resamplingConstants.reservoirsResolution = ComputeReservoirsResolution(resolution);
	resamplingConstants.pixelSize            = resolution.cast<Real32>().cwiseInverse();
	resamplingConstants.frameIdx             = params.renderView.GetRenderedFrameIdx();
	resamplingConstants.resamplingRangeStep  = math::Utils::MapInputToOutputRange(resMinAxis, math::Vector2f(1080.f, 2000.f), math::Vector2f(1.8f, 6.f));

	return resamplingConstants;
}


namespace temporal
{

DS_BEGIN(SRTemporalResamplingDS, rg::RGDescriptorSetState<SRTemporalResamplingDS>)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),                            u_depthTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector2f>),                    u_normalsTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),                            u_roughnessTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector3f>),                    u_specularColorTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector2f>),                    u_motionTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),                            u_historyDepthTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector2f>),                    u_historyNormalsTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),                            u_historyRoughnessTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector3f>),                    u_historySpecularColorTexture)
	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<SRPackedReservoir>),             u_inReservoirsBuffer)
	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<SRPackedReservoir>),             u_historyReservoirsBuffer)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<SRPackedReservoir>),           u_outReservoirsBuffer)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<vrt::EncodedRayTraceCommand>), u_rayTracesCommands)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<Uint32>),                      u_commandsNum)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<Uint32>),                             u_rwVariableRateBlocksTexture)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<Uint32>),                      u_tracesDispatchGroupsNum)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<Uint32>),                      u_tracesNum)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<SRResamplingConstants>),           u_resamplingConstants)
DS_END();


static rdr::PipelineStateID CompileResolveReservoirsPipeline()
{
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/SpecularReflections/ResampleTemporally.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "ResampleTemporallyCS"));

	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("Resample Temporally Pipeline"), shader);
}


vrt::TracesAllocation PrepareAdditionalTracesAllocationData(rg::RenderGraphBuilder& graphBuilder, const ResamplingParams& params)
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


vrt::TracesAllocation ResampleTemporally(rg::RenderGraphBuilder& graphBuilder, const ResamplingParams& params, const SRResamplingConstants& resamplingConstants, utils::ReservoirsState& reservoirsState)
{
	SPT_PROFILER_FUNCTION();

	const vrt::TracesAllocation additionalTracesAllocation = PrepareAdditionalTracesAllocationData(graphBuilder, params);

	const math::Vector2u resolution = params.GetResolution();

	lib::MTHandle<SRTemporalResamplingDS> ds = graphBuilder.CreateDescriptorSet<SRTemporalResamplingDS>(RENDERER_RESOURCE_NAME("Resample Temporally DS"));
	ds->u_depthTexture                = params.depthTexture;
	ds->u_normalsTexture              = params.normalsTexture;
	ds->u_roughnessTexture            = params.roughnessTexture;
	ds->u_specularColorTexture        = params.specularColorTexture;
	ds->u_motionTexture               = params.motionTexture;
	ds->u_historyDepthTexture         = params.historyDepthTexture;
	ds->u_historyNormalsTexture       = params.historyNormalsTexture;
	ds->u_historyRoughnessTexture     = params.historyRoughnessTexture;
	ds->u_historySpecularColorTexture = params.historySpecularColorTexture;
	ds->u_inReservoirsBuffer          = params.initialReservoirBuffer;
	ds->u_historyReservoirsBuffer     = reservoirsState.ReadReservoirs();
	ds->u_outReservoirsBuffer         = reservoirsState.WriteReservoirs();
	ds->u_rayTracesCommands           = additionalTracesAllocation.rayTraceCommands;
	ds->u_commandsNum                 = additionalTracesAllocation.tracingIndirectArgs;
	ds->u_rwVariableRateBlocksTexture = additionalTracesAllocation.variableRateBlocksTexture;
	ds->u_tracesDispatchGroupsNum     = additionalTracesAllocation.dispatchIndirectArgs;
	ds->u_tracesNum                   = additionalTracesAllocation.tracesNum;
	ds->u_resamplingConstants         = resamplingConstants;

	reservoirsState.RollBuffers();

	static const rdr::PipelineStateID pipeline = CompileResolveReservoirsPipeline();

	graphBuilder.Dispatch(RG_DEBUG_NAME("Resample Temporally"),
						  pipeline,
						  math::Utils::DivideCeil(resolution, math::Vector2u(8u, 8u)),
						  rg::BindDescriptorSets(std::move(ds), params.renderView.GetRenderViewDS()));

	return additionalTracesAllocation;
}

} // temporal

namespace spatial
{

DS_BEGIN(SRSpatialResamplingDS, rg::RGDescriptorSetState<SRSpatialResamplingDS>)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),                          u_depthTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector2f>),                  u_normalsTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),                          u_roughnessTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Uint32>),                          u_variableRateBlocksTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector3f>),                  u_specularColorTexture)
	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<SRPackedReservoir>),           u_inReservoirsBuffer)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<SRPackedReservoir>),         u_outReservoirsBuffer)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<SRResamplingConstants>),         u_resamplingConstants)
DS_END();


static rdr::PipelineStateID CompileResolveReservoirsPipeline()
{
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/SpecularReflections/ResampleSpatially.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "ResampleSpatiallyCS"));

	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("Resample Spatially Pipeline"), shader);
}


void ResampleSpatially(rg::RenderGraphBuilder& graphBuilder, const ResamplingParams& params, const SRResamplingConstants& resamplingConstants, utils::ReservoirsState& reservoirsState, Uint32 iteration)
{
	SPT_PROFILER_FUNCTION();

	const math::Vector2u resolution = params.GetResolution();

	lib::MTHandle<SRSpatialResamplingDS> ds = graphBuilder.CreateDescriptorSet<SRSpatialResamplingDS>(RENDERER_RESOURCE_NAME("Resample Spatially DS"));
	ds->u_depthTexture               = params.depthTexture;
	ds->u_normalsTexture             = params.normalsTexture;
	ds->u_roughnessTexture           = params.roughnessTexture;
	ds->u_specularColorTexture       = params.specularColorTexture;
	ds->u_variableRateBlocksTexture  = params.tracesAllocation.variableRateBlocksTexture;
	ds->u_inReservoirsBuffer         = reservoirsState.ReadReservoirs();
	ds->u_outReservoirsBuffer        = reservoirsState.WriteReservoirs();
	ds->u_resamplingConstants        = resamplingConstants;

	reservoirsState.RollBuffers();

	static const rdr::PipelineStateID pipeline = CompileResolveReservoirsPipeline();

	graphBuilder.Dispatch(RG_DEBUG_NAME("Resample Spatially"),
						  pipeline,
						  math::Utils::DivideCeil(resolution, math::Vector2u(8u, 8u)),
						  rg::BindDescriptorSets(std::move(ds), params.renderView.GetRenderViewDS()));
}

} // spatial

namespace final_visibility
{

DS_BEGIN(ApplyInvalidationMaskDS, rg::RGDescriptorSetState<ApplyInvalidationMaskDS>)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<SRPackedReservoir>), u_inOutReservoirsBuffer)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Uint32>),                  u_invalidationMask)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<SRResamplingConstants>), u_resamplingConstants)
DS_END();


static rdr::PipelineStateID CompileApplyInvalidationMaskPipeline()
{
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/SpecularReflections/ApplyInvalidationMask.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "ApplyInvalidationMaskCS"));

	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("Apply Invalidation Mask Pipeline"), shader);
}


void ApplyInvalidationMask(rg::RenderGraphBuilder& graphBuilder, const ResamplingParams& params, const SRResamplingConstants& resamplingConstants, utils::ReservoirsState& reservoirsState, rg::RGTextureViewHandle invalidationMask)
{
	SPT_PROFILER_FUNCTION();

	lib::MTHandle<ApplyInvalidationMaskDS> ds = graphBuilder.CreateDescriptorSet<ApplyInvalidationMaskDS>(RENDERER_RESOURCE_NAME("Apply Invalidation Mask DS"));
	ds->u_inOutReservoirsBuffer = reservoirsState.ReadReservoirs();
	ds->u_invalidationMask      = invalidationMask;
	ds->u_resamplingConstants   = resamplingConstants;

	static const rdr::PipelineStateID pipeline = CompileApplyInvalidationMaskPipeline();

	graphBuilder.Dispatch(RG_DEBUG_NAME("Apply Invalidation Mask"),
						  pipeline,
						  math::Utils::DivideCeil(params.GetResolution(), math::Vector2u(8u, 8u)),
						  rg::BindDescriptorSets(std::move(ds)));

}


DS_BEGIN(SRResamplingFinalVisibilityTestDS, rg::RGDescriptorSetState<SRResamplingFinalVisibilityTestDS>)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),                          u_depthTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector2f>),                  u_normalsTexture)
	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<vrt::EncodedRayTraceCommand>), u_traceCommands)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<SRPackedReservoir>),         u_inOutReservoirsBuffer)
	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<SRPackedReservoir>),           u_initialReservoirsBuffer)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<Uint32>),                           u_rwInvalidationMask)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<SRResamplingConstants>),         u_resamplingConstants)
DS_END();


static rdr::PipelineStateID CompileSRFinalVisibilityTestPipeline(const RayTracingRenderSceneSubsystem& rayTracingSubsystem)
{
	rdr::RayTracingPipelineShaders rtShaders;
	rtShaders.rayGenShader = rdr::ResourcesManager::CreateShader("Sculptor/SpecularReflections/ResamplingFinalVisibility.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::RTGeneration, "ResamplingFinalVisibilityTestRTG"));
	rtShaders.missShaders.emplace_back(rdr::ResourcesManager::CreateShader("Sculptor/SpecularReflections/ResamplingFinalVisibility.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::RTMiss, "RTVisibilityRTM")));

	const lib::HashedString materialTechnique = "RTVisibility";
	rayTracingSubsystem.FillRayTracingGeometryHitGroups(materialTechnique, INOUT rtShaders.hitGroups);

	rhi::RayTracingPipelineDefinition pipelineDefinition;
	pipelineDefinition.maxRayRecursionDepth = 1;
	return rdr::ResourcesManager::CreateRayTracingPipeline(RENDERER_RESOURCE_NAME("SR Final Visibility Test Pipeline"), rtShaders, pipelineDefinition);
}


static void ExecuteFinalVisibilityTest(rg::RenderGraphBuilder& graphBuilder, const ResamplingParams& params, const SRResamplingConstants& resamplingConstants, utils::ReservoirsState& reservoirsState)
{
	SPT_PROFILER_FUNCTION();

	const math::Vector2u resolution = params.GetResolution();

	// Invalidation mask uses R32_UINT format where each bit is reserved for a single pixel, so we need 1 invalidation pixels per 8x4 pixels
	const math::Vector2u invalidationMaskResolution = math::Utils::DivideCeil(resolution, math::Vector2u(8u, 4u));
	rg::RGTextureViewHandle invalidationMask = graphBuilder.CreateTextureView(RG_DEBUG_NAME("Invalidation Mask"),
																			  rg::TextureDef(invalidationMaskResolution, rhi::EFragmentFormat::R32_U_Int));

	graphBuilder.ClearTexture(RG_DEBUG_NAME("Clear Invalidation Mask"), invalidationMask, rhi::ClearColor(0u, 0u, 0u, 0u));

	const RayTracingRenderSceneSubsystem& rayTracingSubsystem = params.renderScene.GetSceneSubsystemChecked<RayTracingRenderSceneSubsystem>();

	lib::MTHandle<SRResamplingFinalVisibilityTestDS> ds = graphBuilder.CreateDescriptorSet<SRResamplingFinalVisibilityTestDS>(RENDERER_RESOURCE_NAME("SR Final Visibility Test DS"));
	ds->u_depthTexture            = params.depthTexture;
	ds->u_normalsTexture          = params.normalsTexture;
	ds->u_traceCommands           = params.tracesAllocation.rayTraceCommands;
	ds->u_inOutReservoirsBuffer   = reservoirsState.ReadReservoirs();
	ds->u_initialReservoirsBuffer = params.initialReservoirBuffer;
	ds->u_rwInvalidationMask      = invalidationMask;
	ds->u_resamplingConstants     = resamplingConstants;

	lib::MTHandle<RTVisibilityDS> rtVisibilityDS = graphBuilder.CreateDescriptorSet<RTVisibilityDS>(RENDERER_RESOURCE_NAME("RT Visibility DS"));
	rtVisibilityDS->u_geometryDS              = GeometryManager::Get().GetGeometryDSState();
	rtVisibilityDS->u_staticMeshUnifiedDataDS = StaticMeshUnifiedData::Get().GetUnifiedDataDS();
	rtVisibilityDS->u_materialsDS             = mat::MaterialsUnifiedData::Get().GetMaterialsDS();
	rtVisibilityDS->u_sceneRayTracingDS       = rayTracingSubsystem.GetSceneRayTracingDS();

	static rdr::PipelineStateID visibilityTestPipeline;
	if (!visibilityTestPipeline.IsValid() || rayTracingSubsystem.AreSBTRecordsDirty())
	{
		visibilityTestPipeline = CompileSRFinalVisibilityTestPipeline(rayTracingSubsystem);
	}

	graphBuilder.TraceRaysIndirect(RG_DEBUG_NAME("SR Final Visibility Test"),
								   visibilityTestPipeline,
								   params.tracesAllocation.tracingIndirectArgs, 0,
								   rg::BindDescriptorSets(std::move(ds),
														  params.renderView.GetRenderViewDS(),
														  std::move(rtVisibilityDS)));

	ApplyInvalidationMask(graphBuilder, params, resamplingConstants, reservoirsState, invalidationMask);
}

} // final_visibility

namespace resolve
{

DS_BEGIN(ResolveReservoirsDS, rg::RGDescriptorSetState<ResolveReservoirsDS>)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),                                   u_depthTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector2f>),                           u_normalsTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),                                   u_roughnessTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector3f>),                           u_specularColorTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Uint32>),                                   u_variableRateBlocksTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector2f>),                           u_brdfIntegrationLUT)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::LinearClampToEdge>), u_brdfIntegrationLUTSampler)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<math::Vector4f>),                            u_luminanceHitDistanceTexture)
	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<SRPackedReservoir>),                    u_reservoirsBuffer)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<SRResamplingConstants>),                  u_resamplingConstants)
DS_END();


static rdr::PipelineStateID CompileResolveReservoirsPipeline()
{
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/SpecularReflections/ResolveReservoirs.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "ResolveReservoirsCS"));

	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("Resolve Reservoirs Pipeline"), shader);
}

static void ResolveReservoirs(rg::RenderGraphBuilder& graphBuilder, const ResamplingParams& params, const SRResamplingConstants& resamplingConstants, utils::ReservoirsState& reservoirsState)
{
	SPT_PROFILER_FUNCTION();

	const math::Vector2u resolution = params.GetResolution();

	lib::MTHandle<ResolveReservoirsDS> ds = graphBuilder.CreateDescriptorSet<ResolveReservoirsDS>(RENDERER_RESOURCE_NAME("Resolve Reservoirs DS"));
	ds->u_depthTexture                        = params.depthTexture;
	ds->u_normalsTexture                      = params.normalsTexture;
	ds->u_roughnessTexture                    = params.roughnessTexture;
	ds->u_specularColorTexture                = params.specularColorTexture;
	ds->u_variableRateBlocksTexture           = params.tracesAllocation.variableRateBlocksTexture;
	ds->u_brdfIntegrationLUT                  = BRDFIntegrationLUT::Get().GetLUT(graphBuilder);
	ds->u_luminanceHitDistanceTexture         = params.outLuminanceHitDistanceTexture;
	ds->u_reservoirsBuffer                    = reservoirsState.ReadReservoirs();
	ds->u_resamplingConstants                 = resamplingConstants;

	static const rdr::PipelineStateID pipeline = CompileResolveReservoirsPipeline();

	graphBuilder.Dispatch(RG_DEBUG_NAME("Resolve Reservoirs"),
						  pipeline,
						  math::Utils::DivideCeil(resolution, math::Vector2u(8u, 8u)),
						  rg::BindDescriptorSets(std::move(ds), params.renderView.GetRenderViewDS()));
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

ResamplingParams::ResamplingParams(const RenderView& inRenderView, const RenderScene& inRenderScene)
	: renderView(inRenderView)
	, renderScene(inRenderScene)
{
}

math::Vector2u ResamplingParams::GetResolution() const
{
	return outLuminanceHitDistanceTexture->GetResolution2D();
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// SpatiotemporalResampler =======================================================================

SpatiotemporalResampler::SpatiotemporalResampler()
	: m_historyResolution(0u, 0u)
{ }

InitialResamplingResult SpatiotemporalResampler::ExecuteInitialResampling(rg::RenderGraphBuilder& graphBuilder, const ResamplingParams& params)
{
	SPT_PROFILER_FUNCTION();

	InitialResamplingResult result;

	const math::Vector2u resolution = params.GetResolution();

	PrepareForResampling(graphBuilder, params);

	const rg::RGBufferViewHandle historyReservoirsBuffer = graphBuilder.AcquireExternalBufferView(m_inputTemporalReservoirBuffer->CreateFullView());
	const rg::RGBufferViewHandle outputReservoirsBuffer  = graphBuilder.AcquireExternalBufferView(m_outputTemporalReservoirBuffer->CreateFullView());

	const SRResamplingConstants resamplingConstants = CreateResamplingConstants(params);

	utils::ReservoirsState reservoirsState(historyReservoirsBuffer, outputReservoirsBuffer);
	if (params.enableTemporalResampling && HasValidTemporalData(params))
	{
		SPT_CHECK(params.historyDepthTexture.IsValid());
		SPT_CHECK(params.historyRoughnessTexture.IsValid());
		SPT_CHECK(params.historySpecularColorTexture.IsValid());
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

	const math::Vector2u resolution = params.GetResolution();

	const rg::RGBufferViewHandle historyReservoirsBuffer = graphBuilder.AcquireExternalBufferView(m_inputTemporalReservoirBuffer->CreateFullView());
	const rg::RGBufferViewHandle outputReservoirsBuffer  = graphBuilder.AcquireExternalBufferView(m_outputTemporalReservoirBuffer->CreateFullView());

	const SRResamplingConstants resamplingConstants = CreateResamplingConstants(params);

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

	for (Uint32 spatialResamplingIteration = 0u; spatialResamplingIteration < params.spatialResamplingIterations; ++spatialResamplingIteration)
	{
		spatial::ResampleSpatially(graphBuilder, params, resamplingConstants, reservoirsState, spatialResamplingIteration);
	}

	if (params.enableFinalVisibilityCheck)
	{
		final_visibility::ExecuteFinalVisibilityTest(graphBuilder, params, resamplingConstants, reservoirsState);
	}

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