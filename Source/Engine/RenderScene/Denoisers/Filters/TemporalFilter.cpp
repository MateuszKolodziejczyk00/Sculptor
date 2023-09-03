#include "TemporalFilter.h"
#include "RGDescriptorSetState.h"
#include "DescriptorSetBindings/RWTextureBinding.h"
#include "DescriptorSetBindings/SRVTextureBinding.h"
#include "DescriptorSetBindings/SamplerBinding.h"
#include "ResourcesManager.h"
#include "RenderGraphBuilder.h"
#include "View/RenderView.h"
#include "ShaderStructs/ShaderStructsMacros.h"


namespace spt::rsc::denoising::filters::temporal
{

BEGIN_SHADER_STRUCT(TemporalFilterShaderParams)
	SHADER_STRUCT_FIELD(Bool, hasValidVarianceTexture)
	SHADER_STRUCT_FIELD(Real32, currentFrameMinWeight)
	SHADER_STRUCT_FIELD(Real32, currentFrameMaxWeight)
	SHADER_STRUCT_FIELD(Real32, varianceHistoryClampMultiplier)
	SHADER_STRUCT_FIELD(Real32, minHistoryClampExtent)
END_SHADER_STRUCT();


DS_BEGIN(TemporalFilterDS, rg::RGDescriptorSetState<TemporalFilterDS>)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<math::Vector4f>),								u_currentTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector4f>),								u_historyTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),										u_historyDepthTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),										u_depthTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector2f>),								u_motionTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector3f>),								u_geometryNormalsTexture)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::NearestClampToEdge>),	u_nearestSampler)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::LinearClampToEdge>),	u_linearSampler)
	DS_BINDING(BINDING_TYPE(gfx::OptionalSRVTexture2DBinding<Real32>),								u_varianceTexture)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableConstantBufferBinding<TemporalFilterShaderParams>),		u_params)
DS_END();


static rdr::PipelineStateID CreateTemporalAccumulationPipeline()
{
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/Denoisers/Filters/TemporalFilter.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "TemporalFilterCS"));
	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("Temporal Filter Pipeline"), shader);
}


void ApplyTemporalFilter(rg::RenderGraphBuilder& graphBuilder, const TemporalFilterParams& params)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(params.HasValidHistory());

	const math::Vector3u resolution = params.currentTexture->GetResolution();

	TemporalFilterShaderParams shaderParams;
	shaderParams.hasValidVarianceTexture		= params.varianceTexture.IsValid();
	shaderParams.currentFrameMinWeight			= params.currentFrameMinWeight;
	shaderParams.currentFrameMaxWeight			= params.currentFrameMaxWeight;
	shaderParams.varianceHistoryClampMultiplier	= params.varianceHistoryClampMultiplier;
	shaderParams.minHistoryClampExtent 			= params.minHistoryClampExtent;

	lib::SharedPtr<TemporalFilterDS> ds = rdr::ResourcesManager::CreateDescriptorSetState<TemporalFilterDS>(RENDERER_RESOURCE_NAME("Temporal Filter DS"));
	ds->u_currentTexture			= params.currentTexture;
	ds->u_historyTexture			= params.historyTexture;
	ds->u_historyDepthTexture		= params.historyDepthTexture;
	ds->u_depthTexture				= params.currentDepthTexture;
	ds->u_motionTexture				= params.motionTexture;
	ds->u_geometryNormalsTexture	= params.geometryNormalsTexture;
	ds->u_varianceTexture			= params.varianceTexture;
	ds->u_params					= shaderParams;

	const rdr::PipelineStateID pipeline = CreateTemporalAccumulationPipeline();

	graphBuilder.Dispatch(RG_DEBUG_NAME(std::format("{}: Denoise Temporal Filter", params.name.Get().ToString())),
						  pipeline,
						  math::Utils::DivideCeil(resolution, math::Vector3u(8u, 8u, 1u)),
						  rg::BindDescriptorSets(std::move(ds),
												 params.renderView.GetRenderViewDS()));
}

} // spt::rsc::denoising::filters::temporal
