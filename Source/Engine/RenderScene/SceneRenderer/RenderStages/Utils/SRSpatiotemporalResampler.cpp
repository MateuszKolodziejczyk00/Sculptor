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


namespace spt::rsc::sr_restir
{

namespace utils
{

class ReservoirsState
{
public:

	ReservoirsState(rg::RenderGraphBuilder& graphBuilder, rg::RGBufferViewHandle initialReservoirs, math::Vector2u resolution)
		: m_initialReservoirsBuffer(std::move(initialReservoirs))
	{
		m_inputReservoirsBuffer  = CreateReservoirsBuffer(graphBuilder, resolution);
		m_outputReservoirsBuffer = CreateReservoirsBuffer(graphBuilder, resolution);
	}

	rg::RGBufferViewHandle ReadReservoirs() const
	{
		return m_initialReservoirsBuffer.IsValid() ? m_initialReservoirsBuffer : m_inputReservoirsBuffer;
	}

	rg::RGBufferViewHandle WriteReservoirs()
	{
		return m_outputReservoirsBuffer;
	}

	void RollBuffers()
	{
		std::swap(m_inputReservoirsBuffer, m_outputReservoirsBuffer);
		m_initialReservoirsBuffer = rg::RGBufferViewHandle();
	}

private:

	rg::RGBufferViewHandle m_inputReservoirsBuffer;
	rg::RGBufferViewHandle m_outputReservoirsBuffer;

	rg::RGBufferViewHandle m_initialReservoirsBuffer;
};

} // utils

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

	const math::Vector2u resolution = params.outLuminanceHitDistanceTexture->GetResolution2D();

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

	const math::Vector2u resolution = params.outLuminanceHitDistanceTexture->GetResolution2D();

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

rg::RGBufferViewHandle CreateReservoirsBuffer(rg::RenderGraphBuilder& graphBuilder, math::Vector2u resolution)
{
	SPT_PROFILER_FUNCTION();

	const Uint64 bufferSize = resolution.x() * resolution.y() * sizeof(SRPackedReservoir);

	rhi::BufferDefinition bufferDef;
	bufferDef.size  = bufferSize;
	bufferDef.usage = rhi::EBufferUsage::Storage;

	return graphBuilder.CreateBufferView(RG_DEBUG_NAME("Reservoir Buffer"), bufferDef, rhi::EMemoryUsage::GPUOnly);
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// SpatiotemporalResampler =======================================================================

SpatiotemporalResampler::SpatiotemporalResampler()
{ }

void SpatiotemporalResampler::Resample(rg::RenderGraphBuilder& graphBuilder, const ResamplingParams& params)
{
	SPT_PROFILER_FUNCTION();

	const math::Vector2u resolution = params.outLuminanceHitDistanceTexture->GetResolution2D();

	utils::ReservoirsState reservoirsState(graphBuilder, params.reservoirBuffer, resolution);

	for (Uint32 spatialResamplingIteration = 0u; spatialResamplingIteration < params.spatialResamplingIterations; ++spatialResamplingIteration)
	{
		spatial::ResampleSpatially(graphBuilder, params, reservoirsState, spatialResamplingIteration);
	}

	resolve::ResolveReservoirs(graphBuilder, params, reservoirsState);
}

} // spt::rsc::sr_restir
