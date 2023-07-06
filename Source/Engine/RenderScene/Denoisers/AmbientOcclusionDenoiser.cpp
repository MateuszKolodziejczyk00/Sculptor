#include "AmbientOcclusionDenoiser.h"
#include "RenderGraphBuilder.h"
#include "View/RenderView.h"
#include "RGDescriptorSetState.h"
#include "DescriptorSetBindings/SRVTextureBinding.h"
#include "DescriptorSetBindings/SamplerBinding.h"
#include "DescriptorSetBindings/RWTextureBinding.h"
#include "ResourcesManager.h"
#include "ShaderStructs/ShaderStructsMacros.h"


namespace spt::rsc
{

namespace ao_denoiser
{

namespace temporal
{

DS_BEGIN(AOTemporalFilterDS, rg::RGDescriptorSetState<AOTemporalFilterDS>)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<Real32>),										u_currentAOTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),										u_historyAOTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),										u_historyDepthTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),										u_depthTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector2f>),								u_motionTexture)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::NearestClampToEdge>),	u_nearestSampler)
DS_END();


static rdr::PipelineStateID CreateTemporalAccumulationPipeline()
{
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/Denoisers/AmbientOcclusionDenoiser/AOTemporalFilter.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "AOTemporalFilterCS"));
	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("AO Temporal Filter Pipeline"), shader);
}


static void ApplyTemporalFilter(rg::RenderGraphBuilder& graphBuilder, const DenoiserParams& params)
{
	SPT_PROFILER_FUNCTION();

	const math::Vector3u resolution = params.currentAOTexture->GetResolution();

	lib::SharedPtr<AOTemporalFilterDS> ds = rdr::ResourcesManager::CreateDescriptorSetState<AOTemporalFilterDS>(RENDERER_RESOURCE_NAME("AO Temporal Filter DS"));
	ds->u_currentAOTexture		= params.currentAOTexture;
	ds->u_historyAOTexture		= params.historyAOTexture;
	ds->u_historyDepthTexture	= params.historyDepthTexture;
	ds->u_depthTexture			= params.currentDepthTexture;
	ds->u_motionTexture			= params.motionTexture;

	const rdr::PipelineStateID pipeline = CreateTemporalAccumulationPipeline();

	graphBuilder.Dispatch(RG_DEBUG_NAME("AO Denoise Temporal Filter"),
						  pipeline,
						  math::Utils::DivideCeil(resolution, math::Vector3u(8u, 8u, 1u)),
						  rg::BindDescriptorSets(std::move(ds),
												 params.renderView.GetRenderViewDS()));
}

} // temporal

namespace spatial
{

BEGIN_SHADER_STRUCT(AOSpatialFilteringParams)
	SHADER_STRUCT_FIELD(Uint32, samplesOffset)
END_SHADER_STRUCT();


DS_BEGIN(AOSpatialFilterDS, rg::RGDescriptorSetState<AOSpatialFilterDS>)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),										u_inputAOTexture)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<Real32>),										u_outputAOTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),										u_depthTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector3f>),								u_geometryNormalsTexture)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::NearestClampToEdge>),	u_nearestSampler)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableConstantBufferBinding<AOSpatialFilteringParams>),			u_params)
DS_END();


static rdr::PipelineStateID CreateSpatialATrousFilterPipeline()
{
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/Denoisers/AmbientOcclusionDenoiser/AOSpatialFilter.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "AOSpatialFilterCS"));
	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("AO Spatial Filter Pipeline"), shader);
}


static void ApplyATrousFilter(rg::RenderGraphBuilder& graphBuilder, const DenoiserParams& params, rg::RGTextureViewHandle input, rg::RGTextureViewHandle output, Uint32 iteration)
{
	SPT_PROFILER_FUNCTION();

	const math::Vector3u resolution = output->GetResolution();

	static const rdr::PipelineStateID pipeline = CreateSpatialATrousFilterPipeline();

	AOSpatialFilteringParams dispatchParams;
	dispatchParams.samplesOffset = 1u << iteration;

	lib::SharedPtr<AOSpatialFilterDS> ds = rdr::ResourcesManager::CreateDescriptorSetState<AOSpatialFilterDS>(RENDERER_RESOURCE_NAME("AO Spatial Filter DS"));
	ds->u_inputAOTexture			= input;
	ds->u_outputAOTexture			= output;
	ds->u_depthTexture				= params.currentDepthTexture;
	ds->u_geometryNormalsTexture	= params.geometryNormalsTexture;
	ds->u_params					= dispatchParams;

	graphBuilder.Dispatch(RG_DEBUG_NAME("AO Denoise A-Trous Filter"),
						  pipeline,
						  math::Utils::DivideCeil(resolution, math::Vector3u(8u, 8u, 1u)),
						  rg::BindDescriptorSets(std::move(ds), params.renderView.GetRenderViewDS()));
}


static void ApplySpatialFilter(rg::RenderGraphBuilder& graphBuilder, const DenoiserParams& params)
{
	SPT_PROFILER_FUNCTION();

	const rhi::TextureDefinition& textureDef = params.currentAOTexture->GetTextureDefinition();
	const rg::RGTextureViewHandle tempTexture = graphBuilder.CreateTextureView(RG_DEBUG_NAME("AO Denoise Temp Texture"), textureDef, rhi::EMemoryUsage::GPUOnly);

	ApplyATrousFilter(graphBuilder, params, params.currentAOTexture, params.historyAOTexture, 0);
	ApplyATrousFilter(graphBuilder, params, params.historyAOTexture, params.currentAOTexture, 1);
	ApplyATrousFilter(graphBuilder, params, params.currentAOTexture, tempTexture, 2);
	ApplyATrousFilter(graphBuilder, params, tempTexture, params.currentAOTexture, 3);
}

} // spatial

void Denoise(rg::RenderGraphBuilder& graphBuilder, const DenoiserParams& params)
{
	SPT_PROFILER_FUNCTION();

	if (params.hasValidHistory)
	{
		temporal::ApplyTemporalFilter(graphBuilder, params);
	}

	spatial::ApplySpatialFilter(graphBuilder, params);
}

} // ao_denoiser

} // spt::rsc
