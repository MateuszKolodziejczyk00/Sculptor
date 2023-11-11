#include "VisibilitySpatialATrousFilter.h"
#include "ShaderStructs/ShaderStructsMacros.h"
#include "DescriptorSetBindings/SRVTextureBinding.h"
#include "DescriptorSetBindings/RWTextureBinding.h"
#include "DescriptorSetBindings/ConstantBufferBinding.h"
#include "DescriptorSetBindings/SamplerBinding.h"
#include "RGDescriptorSetState.h"
#include "ResourcesManager.h"
#include "RenderGraphBuilder.h"
#include "View/RenderView.h"


namespace spt::rsc::visibility_denoiser::spatial
{

BEGIN_SHADER_STRUCT(SpatialATrousFilteringParams)
	SHADER_STRUCT_FIELD(Uint32, samplesOffset)
	SHADER_STRUCT_FIELD(Uint32, hasValidMomentsTexture)
END_SHADER_STRUCT();


DS_BEGIN(SpatialATrousFilterDS, rg::RGDescriptorSetState<SpatialATrousFilterDS>)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),										u_inputTexture)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<Real32>),										u_outputTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),										u_depthTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector3f>),								u_normalsTexture)
	DS_BINDING(BINDING_TYPE(gfx::OptionalSRVTexture2DBinding<Real32>),								u_momentsTexture)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::NearestClampToEdge>),	u_nearestSampler)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableConstantBufferBinding<SpatialATrousFilteringParams>),		u_params)
DS_END();


static rdr::PipelineStateID CreateSpatialATrousFilterPipeline()
{
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/Denoisers/Visibility/VisibilitySpatialATrousFilter.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "SpatialATrousFilterCS"));
	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("Visibility Spatial A-Trous Filter Pipeline"), shader);
}


void ApplyATrousFilter(rg::RenderGraphBuilder& graphBuilder, const SpatialATrousFilterParams& params, rg::RGTextureViewHandle input, rg::RGTextureViewHandle output, Uint32 iterationIdx)
{
	SPT_PROFILER_FUNCTION();

	const math::Vector3u resolution = output->GetResolution();

	static const rdr::PipelineStateID pipeline = CreateSpatialATrousFilterPipeline();

	SpatialATrousFilteringParams dispatchParams;
	dispatchParams.samplesOffset			= 1u << iterationIdx;
	dispatchParams.hasValidMomentsTexture	= params.momentsTexture.IsValid();

	lib::MTHandle<SpatialATrousFilterDS> ds = graphBuilder.CreateDescriptorSet<SpatialATrousFilterDS>(RENDERER_RESOURCE_NAME("Spatial A-Trous Filter DS"));
	ds->u_inputTexture                      = input;
	ds->u_outputTexture                     = output;
	ds->u_depthTexture                      = params.depthTexture;
	ds->u_normalsTexture                    = params.normalsTexture;
	ds->u_momentsTexture                    = params.momentsTexture;
	ds->u_params                            = dispatchParams;

	graphBuilder.Dispatch(RG_DEBUG_NAME(std::format("{}: Denoise Spatial A-Trous Filter (Iteration {})", params.name.Get().ToString(), iterationIdx)),
						  pipeline,
						  math::Utils::DivideCeil(resolution, math::Vector3u(8u, 8u, 1u)),
						  rg::BindDescriptorSets(std::move(ds), params.renderView.GetRenderViewDS()));
}

} // spt::rsc::visibility_denoiser::spatial
