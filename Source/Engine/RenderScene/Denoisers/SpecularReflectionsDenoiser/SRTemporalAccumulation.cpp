
#include "SRTemporalAccumulation.h"
#include "RGDescriptorSetState.h"
#include "DescriptorSetBindings/RWTextureBinding.h"
#include "DescriptorSetBindings/SRVTextureBinding.h"
#include "DescriptorSetBindings/SamplerBinding.h"
#include "DescriptorSetBindings/ConstantBufferBinding.h"
#include "ResourcesManager.h"
#include "RenderGraphBuilder.h"
#include "View/RenderView.h"
#include "DDGI/DDGITypes.h"
#include "RenderScene.h"
#include "EngineFrame.h"


namespace spt::rsc::sr_denoiser
{

BEGIN_SHADER_STRUCT(SRTemporalAccumulationConstants)
	SHADER_STRUCT_FIELD(Real32, deltaTime)
END_SHADER_STRUCT();


DS_BEGIN(SRTemporalAccumulationDS, rg::RGDescriptorSetState<SRTemporalAccumulationDS>)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<Uint32>),                                     u_accumulatedSamplesNumTexture)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<Real32>),                                     u_reprojectionConfidenceTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),                                    u_historyDepthTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),                                    u_depthTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector2f>),                            u_motionTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector2f>),                            u_normalsTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector2f>),                            u_historyNormalsTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),                                    u_historyRoughnessTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),                                    u_roughnessTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Uint32>),                                    u_historyAccumulatedSamplesNumTexture)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<math::Vector4f>),                             u_rwSpecularTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector4f>),                            u_specularHistoryTexture)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<math::Vector2f>),                             u_rwSpecularTemporalVarianceTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector2f>),                            u_specularHistoryTemporalVarianceTexture)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<math::Vector3f>),                             u_rwSpecularFastHistoryTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector3f>),                            u_specularFastHistoryTexture)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<math::Vector4f>),                             u_rwDiffuseTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector4f>),                            u_diffuseHistoryTexture)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<math::Vector2f>),                             u_rwDiffuseTemporalVarianceTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector2f>),                            u_diffuseHistoryTemporalVarianceTexture)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<math::Vector3f>),                             u_rwDiffuseFastHistoryTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector3f>),                            u_diffuseFastHistoryTexture)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::NearestClampToEdge>), u_nearestSampler)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::LinearClampToEdge>),  u_linearSampler)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<SRTemporalAccumulationConstants>),         u_constants)
DS_END();


static rdr::PipelineStateID CreateTemporalAccumulationPipeline()
{
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/SpecularReflections/Denoiser/SRTemporalAccumulation.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "SRTemporalAccumulationCS"));
	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("Specular Reflections Temporal Accumulation Pipeline"), shader);
}


rg::RGTextureViewHandle CreateReprojectionConfidenceTexture(rg::RenderGraphBuilder& graphBuilder, math::Vector2u resolution)
{
	return graphBuilder.CreateTextureView(RG_DEBUG_NAME("Reprojection Confidence"), rg::TextureDef(resolution, rhi::EFragmentFormat::R16_UN_Float));
}


void ApplyTemporalAccumulation(rg::RenderGraphBuilder& graphBuilder, const TemporalAccumulationParameters& params)
{
	SPT_PROFILER_FUNCTION();

	const math::Vector2u resolution = params.currentSpecularTexture->GetResolution2D();

	const RenderScene& renderScene = params.renderView.GetRenderScene();
	const engn::FrameContext& currentFrame = renderScene.GetCurrentFrameRef();

	SRTemporalAccumulationConstants shaderConstants;
	shaderConstants.deltaTime = currentFrame.GetDeltaTime();

	lib::MTHandle<SRTemporalAccumulationDS> ds = graphBuilder.CreateDescriptorSet<SRTemporalAccumulationDS>(RENDERER_RESOURCE_NAME("SRTemporalAccumulationDS"));
	ds->u_historyDepthTexture                    = params.historyDepthTexture;
	ds->u_depthTexture                           = params.currentDepthTexture;
	ds->u_motionTexture	                         = params.motionTexture;
	ds->u_normalsTexture                         = params.normalsTexture;
	ds->u_historyNormalsTexture                  = params.historyNormalsTexture;
	ds->u_historyRoughnessTexture                = params.historyRoughnessTexture;
	ds->u_roughnessTexture                       = params.currentRoughnessTexture;
	ds->u_accumulatedSamplesNumTexture           = params.accumulatedSamplesNumTexture;
	ds->u_historyAccumulatedSamplesNumTexture    = params.historyAccumulatedSamplesNumTexture;
	ds->u_reprojectionConfidenceTexture          = params.reprojectionConfidenceTexture;
	ds->u_rwSpecularTexture                      = params.currentSpecularTexture;
	ds->u_specularHistoryTexture                 = params.historySpecularTexture;
	ds->u_rwSpecularTemporalVarianceTexture      = params.temporalVarianceSpecularTexture;
	ds->u_specularHistoryTemporalVarianceTexture = params.historyTemporalVarianceSpecularTexture;
	ds->u_rwSpecularFastHistoryTexture           = params.fastHistorySpecularTexture;
	ds->u_specularFastHistoryTexture             = params.fastHistorySpecularOutputTexture;
	ds->u_rwDiffuseTexture                       = params.currentDiffuseTexture;
	ds->u_diffuseHistoryTexture                  = params.historyDiffuseTexture;
	ds->u_rwDiffuseTemporalVarianceTexture       = params.temporalVarianceDiffuseTexture;
	ds->u_diffuseHistoryTemporalVarianceTexture  = params.historyTemporalVarianceDiffuseTexture;
	ds->u_rwDiffuseFastHistoryTexture            = params.fastHistoryDiffuseTexture;
	ds->u_diffuseFastHistoryTexture              = params.fastHistoryDiffuseOutputTexture;
	ds->u_constants                              = shaderConstants;

	static const rdr::PipelineStateID pipeline = CreateTemporalAccumulationPipeline();

	graphBuilder.Dispatch(RG_DEBUG_NAME_FORMATTED("{}: SR Temporal Accumulation", params.name.AsString()),
						  pipeline,
						  math::Utils::DivideCeil(resolution, math::Vector2u(8u, 8u)),
						  rg::BindDescriptorSets(std::move(ds),
												 params.renderView.GetRenderViewDS()));
}

} // spt::rsc::sr_denoiser
