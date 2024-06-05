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

namespace temporal
{

BEGIN_SHADER_STRUCT(SRTemporalResamplingConstants)
	SHADER_STRUCT_FIELD(math::Vector2u, resolution)
	SHADER_STRUCT_FIELD(math::Vector2f, pixelSize)
	SHADER_STRUCT_FIELD(Uint32,         frameIdx)
END_SHADER_STRUCT();


DS_BEGIN(SRTemporalResamplingDS, rg::RGDescriptorSetState<SRTemporalResamplingDS>)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),                          u_depthTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector3f>),                  u_normalsTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),                          u_roughnessTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector3f>),                  u_specularColorTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector2f>),                  u_motionTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),                          u_historyDepthTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector3f>),                  u_historyNormalsTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),                          u_historyRoughnessTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector3f>),                  u_historySpecularColorTexture)
	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<SRPackedReservoir>),           u_inReservoirsBuffer)
	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<SRPackedReservoir>),           u_historyReservoirsBuffer)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<SRPackedReservoir>),         u_outReservoirsBuffer)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<SRTemporalResamplingConstants>), u_resamplingConstants)
DS_END();


static rdr::PipelineStateID CompileResolveReservoirsPipeline()
{
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/SpecularReflections/ResampleTemporally.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "ResampleTemporallyCS"));

	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("Resample Temporally Pipeline"), shader);
}


void ResampleTemporally(rg::RenderGraphBuilder& graphBuilder, const ResamplingParams& params, utils::ReservoirsState& reservoirsState)
{
	SPT_PROFILER_FUNCTION();

	const math::Vector2u resolution = params.GetResolution();

	SRTemporalResamplingConstants shaderConstants;
	shaderConstants.resolution = resolution;
	shaderConstants.pixelSize  = resolution.cast<Real32>().cwiseInverse();
	shaderConstants.frameIdx   = params.renderView.GetRenderedFrameIdx();

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
	ds->u_resamplingConstants         = shaderConstants;

	reservoirsState.RollBuffers();

	static const rdr::PipelineStateID pipeline = CompileResolveReservoirsPipeline();

	graphBuilder.Dispatch(RG_DEBUG_NAME("Resample Temporally"),
						  pipeline,
						  math::Utils::DivideCeil(resolution, math::Vector2u(8u, 8u)),
						  rg::BindDescriptorSets(std::move(ds), params.renderView.GetRenderViewDS()));
}

} // temporal

namespace spatial
{

BEGIN_SHADER_STRUCT(SRSpatialResamplingConstants)
	SHADER_STRUCT_FIELD(math::Vector2u, resolution)
	SHADER_STRUCT_FIELD(math::Vector2f, pixelSize)
	SHADER_STRUCT_FIELD(Uint32,         frameIdx)
END_SHADER_STRUCT();


DS_BEGIN(SRSpatialResamplingDS, rg::RGDescriptorSetState<SRSpatialResamplingDS>)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),                         u_depthTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector3f>),                 u_normalsTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),                         u_roughnessTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector3f>),                 u_specularColorTexture)
	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<SRPackedReservoir>),          u_inReservoirsBuffer)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<SRPackedReservoir>),        u_outReservoirsBuffer)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<SRSpatialResamplingConstants>), u_resamplingConstants)
DS_END();


static rdr::PipelineStateID CompileResolveReservoirsPipeline()
{
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/SpecularReflections/ResampleSpatially.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "ResampleSpatiallyCS"));

	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("Resample Spatially Pipeline"), shader);
}


void ResampleSpatially(rg::RenderGraphBuilder& graphBuilder, const ResamplingParams& params, utils::ReservoirsState& reservoirsState, Uint32 iteration)
{
	SPT_PROFILER_FUNCTION();

	const math::Vector2u resolution = params.GetResolution();

	SRSpatialResamplingConstants shaderConstants;
	shaderConstants.resolution = resolution;
	shaderConstants.pixelSize  = resolution.cast<Real32>().cwiseInverse();
	shaderConstants.frameIdx   = params.renderView.GetRenderedFrameIdx() + iteration * 523u;

	lib::MTHandle<SRSpatialResamplingDS> ds = graphBuilder.CreateDescriptorSet<SRSpatialResamplingDS>(RENDERER_RESOURCE_NAME("Resample Spatially DS"));
	ds->u_depthTexture         = params.depthTexture;
	ds->u_normalsTexture       = params.normalsTexture;
	ds->u_roughnessTexture     = params.roughnessTexture;
	ds->u_specularColorTexture = params.specularColorTexture;
	ds->u_inReservoirsBuffer   = reservoirsState.ReadReservoirs();
	ds->u_outReservoirsBuffer  = reservoirsState.WriteReservoirs();
	ds->u_resamplingConstants  = shaderConstants;

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

BEGIN_SHADER_STRUCT(SRResamplingFinalVisibilityConstants)
	SHADER_STRUCT_FIELD(math::Vector2u, resolution)
	SHADER_STRUCT_FIELD(math::Vector2f, pixelSize)
END_SHADER_STRUCT();


DS_BEGIN(SRResamplingFinalVisibilityTestDS, rg::RGDescriptorSetState<SRResamplingFinalVisibilityTestDS>)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),                                 u_depthTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector3f>),                         u_normalsTexture)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<SRPackedReservoir>),                u_inOutReservoirsBuffer)
	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<SRPackedReservoir>),                  u_initialReservoirsBuffer)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<SRResamplingFinalVisibilityConstants>), u_shaderConstants)
	DS_BINDING(BINDING_TYPE(gfx::AccelerationStructureBinding),                                u_worldAccelerationStructure)
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

static void ExecuteFinalVisibilityTest(rg::RenderGraphBuilder& graphBuilder, const ResamplingParams& params, utils::ReservoirsState& reservoirsState)
{
	SPT_PROFILER_FUNCTION();

	const math::Vector2u resolution = params.GetResolution();

	const RayTracingRenderSceneSubsystem& rayTracingSubsystem = params.renderScene.GetSceneSubsystemChecked<RayTracingRenderSceneSubsystem>();

	SRResamplingFinalVisibilityConstants shaderConstants;
	shaderConstants.resolution = resolution;
	shaderConstants.pixelSize  = resolution.cast<Real32>().cwiseInverse();

	lib::MTHandle<SRResamplingFinalVisibilityTestDS> ds = graphBuilder.CreateDescriptorSet<SRResamplingFinalVisibilityTestDS>(RENDERER_RESOURCE_NAME("SR Final Visibility Test DS"));
	ds->u_depthTexture               = params.depthTexture;
	ds->u_normalsTexture             = params.normalsTexture;
	ds->u_inOutReservoirsBuffer      = params.initialReservoirBuffer;
	ds->u_initialReservoirsBuffer    = reservoirsState.ReadReservoirs();
	ds->u_shaderConstants            = shaderConstants;
	ds->u_worldAccelerationStructure = lib::Ref(rayTracingSubsystem.GetSceneTLAS());

	lib::MTHandle<RTVisibilityDS> rtVisibilityDS = graphBuilder.CreateDescriptorSet<RTVisibilityDS>(RENDERER_RESOURCE_NAME("RT Visibility DS"));
	rtVisibilityDS->u_rtInstances             = rayTracingSubsystem.GetRTInstancesDataBuffer()->CreateFullView();
	rtVisibilityDS->u_geometryDS              = GeometryManager::Get().GetGeometryDSState();
	rtVisibilityDS->u_staticMeshUnifiedDataDS = StaticMeshUnifiedData::Get().GetUnifiedDataDS();
	rtVisibilityDS->u_MaterialsDS             = mat::MaterialsUnifiedData::Get().GetMaterialsDS();

	static rdr::PipelineStateID visibilityTestPipeline;
	if (!visibilityTestPipeline.IsValid() || rayTracingSubsystem.AreSBTRecordsDirty())
	{
		visibilityTestPipeline = CompileSRFinalVisibilityTestPipeline(rayTracingSubsystem);
	}

	graphBuilder.TraceRays(RG_DEBUG_NAME("SR Final Visibility Test"),
						  visibilityTestPipeline,
						  resolution,
						  rg::BindDescriptorSets(std::move(ds),
												 params.renderView.GetRenderViewDS(),
												 std::move(rtVisibilityDS)));
}

} // final_visibility

namespace resolve
{

BEGIN_SHADER_STRUCT(ResolveReservoirsConstants)
	SHADER_STRUCT_FIELD(math::Vector2u, resolution)
	SHADER_STRUCT_FIELD(math::Vector2f, pixelSize)
END_SHADER_STRUCT();


DS_BEGIN(ResolveReservoirsDS, rg::RGDescriptorSetState<ResolveReservoirsDS>)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),                                   u_depthTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector3f>),                           u_normalsTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),                                   u_roughnessTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector3f>),                           u_specularColorTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector2f>),                           u_brdfIntegrationLUT)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::LinearClampToEdge>), u_brdfIntegrationLUTSampler)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<math::Vector4f>),                            u_luminanceHitDistanceTexture)
	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<SRPackedReservoir>),                    u_reservoirsBuffer)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<ResolveReservoirsConstants>),             u_resolveConstants)
DS_END();


static rdr::PipelineStateID CompileResolveReservoirsPipeline()
{
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/SpecularReflections/ResolveReservoirs.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "ResolveReservoirsCS"));

	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("Resolve Reservoirs Pipeline"), shader);
}

static void ResolveReservoirs(rg::RenderGraphBuilder& graphBuilder, const ResamplingParams& params, utils::ReservoirsState& reservoirsState)
{
	SPT_PROFILER_FUNCTION();

	const math::Vector2u resolution = params.GetResolution();

	ResolveReservoirsConstants shaderConstants;
	shaderConstants.resolution = resolution;
	shaderConstants.pixelSize  = resolution.cast<Real32>().cwiseInverse();

	lib::MTHandle<ResolveReservoirsDS> ds = graphBuilder.CreateDescriptorSet<ResolveReservoirsDS>(RENDERER_RESOURCE_NAME("Resolve Reservoirs DS"));
	ds->u_depthTexture                = params.depthTexture;
	ds->u_normalsTexture              = params.normalsTexture;
	ds->u_roughnessTexture            = params.roughnessTexture;
	ds->u_specularColorTexture        = params.specularColorTexture;
	ds->u_brdfIntegrationLUT          = BRDFIntegrationLUT::Get().GetLUT(graphBuilder);
	ds->u_luminanceHitDistanceTexture = params.outLuminanceHitDistanceTexture;
	ds->u_reservoirsBuffer            = reservoirsState.ReadReservoirs();
	ds->u_resolveConstants            = shaderConstants;

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

rg::RGBufferViewHandle CreateReservoirsBuffer(rg::RenderGraphBuilder& graphBuilder, math::Vector2u resolution)
{
	SPT_PROFILER_FUNCTION();

	const Uint64 bufferSize = ComputeReservoirsBufferSize(resolution);

	rhi::BufferDefinition bufferDef;
	bufferDef.size  = bufferSize;
	bufferDef.usage = rhi::EBufferUsage::Storage;

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

void SpatiotemporalResampler::Resample(rg::RenderGraphBuilder& graphBuilder, const ResamplingParams& params)
{
	SPT_PROFILER_FUNCTION();

	const math::Vector2u resolution = params.GetResolution();

	PrepareForResampling(graphBuilder, params);

	const rg::RGBufferViewHandle historyReservoirsBuffer = graphBuilder.AcquireExternalBufferView(m_inputTemporalReservoirBuffer->CreateFullView());
	const rg::RGBufferViewHandle outputReservoirsBuffer  = graphBuilder.AcquireExternalBufferView(m_outputTemporalReservoirBuffer->CreateFullView());

	utils::ReservoirsState reservoirsState(historyReservoirsBuffer, outputReservoirsBuffer);
	if (params.enableTemporalResampling && HasValidTemporalData(params))
	{
		SPT_CHECK(params.historyDepthTexture.IsValid());
		SPT_CHECK(params.historyRoughnessTexture.IsValid());
		SPT_CHECK(params.historySpecularColorTexture.IsValid());
		SPT_CHECK(params.historyNormalsTexture.IsValid());

		temporal::ResampleTemporally(graphBuilder, params, reservoirsState);
	}
	else
	{
		// if there's no temporal data, spatial pass should initially read from the initial reservoirs buffer
		reservoirsState.ReadNextFromInitialReservoirs(params.initialReservoirBuffer);
	}

	for (Uint32 spatialResamplingIteration = 0u; spatialResamplingIteration < params.spatialResamplingIterations; ++spatialResamplingIteration)
	{
		spatial::ResampleSpatially(graphBuilder, params, reservoirsState, spatialResamplingIteration);
	}

	if (params.enableFinalVisibilityCheck)
	{
		final_visibility::ExecuteFinalVisibilityTest(graphBuilder, params, reservoirsState);
	}

	resolve::ResolveReservoirs(graphBuilder, params, reservoirsState);

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
		m_inputTemporalReservoirBuffer  = CreateTemporalReservoirBuffer(graphBuilder, resolution);
		m_outputTemporalReservoirBuffer = CreateTemporalReservoirBuffer(graphBuilder, resolution);
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
