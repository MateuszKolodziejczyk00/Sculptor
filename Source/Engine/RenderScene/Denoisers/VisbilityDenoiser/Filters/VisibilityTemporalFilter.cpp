#include "VisibilityTemporalFilter.h"
#include "RGDescriptorSetState.h"
#include "DescriptorSetBindings/RWTextureBinding.h"
#include "DescriptorSetBindings/SRVTextureBinding.h"
#include "DescriptorSetBindings/SamplerBinding.h"
#include "ResourcesManager.h"
#include "RenderGraphBuilder.h"
#include "View/RenderView.h"
#include "ShaderStructs/ShaderStructsMacros.h"


namespace spt::rsc::visibility_denoiser::temporal_accumulation
{

BEGIN_SHADER_STRUCT(TemporalFilterShaderParams)
	SHADER_STRUCT_FIELD(Real32, currentFrameDefaultWeight)
	SHADER_STRUCT_FIELD(Real32, accumulatedFramesMaxCount)
END_SHADER_STRUCT();


DS_BEGIN(VisibilityTemporalFilterDS, rg::RGDescriptorSetState<VisibilityTemporalFilterDS>)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<Real32>),                                     u_varianceTexture)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<Real32>),                                     u_currentTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),                                    u_historyTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),                                    u_historyDepthTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),                                    u_depthTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector2f>),                            u_motionTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector3f>),                            u_normalsTexture)
	DS_BINDING(BINDING_TYPE(gfx::OptionalRWTexture2DBinding<Uint32>),                             u_accumulatedSamplesNumTexture)
	DS_BINDING(BINDING_TYPE(gfx::OptionalSRVTexture2DBinding<Uint32>),                            u_accumulatedSamplesNumHistoryTexture)
	DS_BINDING(BINDING_TYPE(gfx::OptionalSRVTexture2DBinding<Real32>),                            u_spatialMomentsTexture)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<math::Vector2f>),                             u_temporalMomentsTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector2f>),                            u_temporalMomentsHistoryTexture)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::NearestClampToEdge>), u_nearestSampler)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::LinearClampToEdge>),  u_linearSampler)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<TemporalFilterShaderParams>),              u_params)
DS_END();


static rdr::PipelineStateID CreateTemporalAccumulationPipeline()
{
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/Denoisers/Visibility/VisibilityTemporalFilter.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "TemporalFilterCS"));
	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("Visibility Temporal Filter Pipeline"), shader);
}


void ApplyTemporalFilter(rg::RenderGraphBuilder& graphBuilder, const TemporalFilterParams& params)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(params.HasValidHistory());
	SPT_CHECK(params.accumulatedSamplesNumTexture.IsValid());
	SPT_CHECK(params.accumulatedSamplesNumHistoryTexture.IsValid());

	const math::Vector3u resolution = params.currentTexture->GetResolution();

	TemporalFilterShaderParams shaderParams;
	shaderParams.currentFrameDefaultWeight = params.currentFrameDefaultWeight;
	shaderParams.accumulatedFramesMaxCount = params.accumulatedFramesMaxCount;

	lib::MTHandle<VisibilityTemporalFilterDS> ds = graphBuilder.CreateDescriptorSet<VisibilityTemporalFilterDS>(RENDERER_RESOURCE_NAME("Visibility Temporal Filter DS"));
	ds->u_currentTexture                      = params.currentTexture;
	ds->u_varianceTexture                     = params.varianceTexture;
	ds->u_historyTexture                      = params.historyTexture;
	ds->u_historyDepthTexture                 = params.historyDepthTexture;
	ds->u_depthTexture                        = params.currentDepthTexture;
	ds->u_motionTexture                       = params.motionTexture;
	ds->u_normalsTexture                      = params.normalsTexture;
	ds->u_accumulatedSamplesNumTexture        = params.accumulatedSamplesNumTexture;
	ds->u_accumulatedSamplesNumHistoryTexture = params.accumulatedSamplesNumHistoryTexture;
	ds->u_spatialMomentsTexture               = params.spatialMomentsTexture;
	ds->u_temporalMomentsTexture              = params.temporalMomentsTexture;
	ds->u_temporalMomentsHistoryTexture       = params.temporalMomentsHistoryTexture;
	ds->u_params                              = shaderParams;

	const rdr::PipelineStateID pipeline = CreateTemporalAccumulationPipeline();

	graphBuilder.Dispatch(RG_DEBUG_NAME(std::format("{}: Denoise Temporal Filter", params.name.Get().ToString())),
						  pipeline,
						  math::Utils::DivideCeil(resolution, math::Vector3u(8u, 8u, 1u)),
						  rg::BindDescriptorSets(std::move(ds),
												 params.renderView.GetRenderViewDS()));
}

} // spt::rsc::visibility_denoiser::temporal_accumulation
