#include "SRTemporalAccumulation.h"
#include "RGDescriptorSetState.h"
#include "DescriptorSetBindings/RWTextureBinding.h"
#include "DescriptorSetBindings/SRVTextureBinding.h"
#include "DescriptorSetBindings/SamplerBinding.h"
#include "ResourcesManager.h"
#include "RenderGraphBuilder.h"
#include "View/RenderView.h"
#include "DDGI/DDGITypes.h"


namespace spt::rsc::sr_denoiser
{

DS_BEGIN(SRTemporalAccumulationDS, rg::RGDescriptorSetState<SRTemporalAccumulationDS>)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<math::Vector4f>),                             u_currentTexture)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<Uint32>),                                     u_accumulatedSamplesNumTexture)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<math::Vector2f>),                             u_temporalVarianceTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector4f>),                            u_historyTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),                                    u_historyDepthTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),                                    u_depthTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector2f>),                            u_motionTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector3f>),                            u_normalsTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector3f>),                            u_historyNormalsTexture)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<Real32>),                                     u_destRoughnessTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),                                    u_historyRoughnessTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector4f>),                            u_specularColorRoughnessTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Uint32>),                                    u_historyAccumulatedSamplesNumTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector2f>),                            u_historyTemporalVarianceTexture)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::NearestClampToEdge>), u_nearestSampler)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::LinearClampToEdge>),  u_linearSampler)
DS_END();


static rdr::PipelineStateID CreateTemporalAccumulationPipeline()
{
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/SpecularReflections/Denoiser/SRTemporalAccumulation.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "SRTemporalAccumulationCS"));
	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("Specular Reflections Temporal Accumulation Pipeline"), shader);
}


void ApplyTemporalAccumulation(rg::RenderGraphBuilder& graphBuilder, const TemporalAccumulationParameters& params)
{
	SPT_PROFILER_FUNCTION();

	const math::Vector2u resolution = params.currentTexture->GetResolution2D();

	static const rdr::PipelineStateID pipeline = CreateTemporalAccumulationPipeline();

	lib::MTHandle<SRTemporalAccumulationDS> ds = graphBuilder.CreateDescriptorSet<SRTemporalAccumulationDS>(RENDERER_RESOURCE_NAME("SRTemporalAccumulationDS"));
	ds->u_currentTexture                       = params.currentTexture;
	ds->u_historyTexture                       = params.historyTexture;
	ds->u_historyDepthTexture                  = params.historyDepthTexture;
	ds->u_depthTexture                         = params.currentDepthTexture;
	ds->u_motionTexture	                       = params.motionTexture;
	ds->u_normalsTexture                       = params.normalsTexture;
	ds->u_historyNormalsTexture                = params.historyNormalsTexture;
	ds->u_destRoughnessTexture                 = params.outputRoughnessTexture;
	ds->u_historyRoughnessTexture              = params.historyRoughnessTexture;
	ds->u_specularColorRoughnessTexture        = params.specularColorAndRoughnessTexture;
	ds->u_accumulatedSamplesNumTexture         = params.accumulatedSamplesNumTexture;
	ds->u_historyAccumulatedSamplesNumTexture  = params.historyAccumulatedSamplesNumTexture;
	ds->u_temporalVarianceTexture              = params.temporalVarianceTexture;
	ds->u_historyTemporalVarianceTexture       = params.historyTemporalVarianceTexture;

	graphBuilder.Dispatch(RG_DEBUG_NAME_FORMATTED("{}: SR Temporal Accumulation", params.name.AsString()),
						  pipeline,
						  math::Utils::DivideCeil(resolution, math::Vector2u(8u, 8u)),
						  rg::BindDescriptorSets(std::move(ds),
												 params.renderView.GetRenderViewDS(),
												 params.ddgiSceneDS));
}

} // spt::rsc::sr_denoiser
