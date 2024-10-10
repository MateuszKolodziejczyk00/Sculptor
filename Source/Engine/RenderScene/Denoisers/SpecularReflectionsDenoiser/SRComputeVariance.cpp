#include "SRComputeVariance.h"
#include "RenderGraphBuilder.h"
#include "RGDescriptorSetState.h"
#include "DescriptorSetBindings/RWTextureBinding.h"
#include "DescriptorSetBindings/SRVTextureBinding.h"
#include "ResourcesManager.h"
#include "View/RenderView.h"


namespace spt::rsc::sr_denoiser
{

DS_BEGIN(SRComputeVarianceDS, rg::RGDescriptorSetState<SRComputeVarianceDS>)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<Real32>),          u_rwVarianceTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Uint32>),         u_accumulatedSamplesNumTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),         u_depthTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector2f>), u_momentsTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector2f>), u_normalsTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector3f>), u_luminanceTexture)
DS_END();


static rdr::PipelineStateID CreateComputeVariancePipeline()
{
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/SpecularReflections/Denoiser/SRComputeVariance.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "SRComputeVarianceCS"));
	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("Specular Reflections Compute Std Dev Pipeline"), shader);
}

rg::RGTextureViewHandle CreateVarianceTexture(rg::RenderGraphBuilder& graphBuilder, math::Vector2u resolution)
{
	return graphBuilder.CreateTextureView(RG_DEBUG_NAME("SR Variance"), rg::TextureDef(resolution, rhi::EFragmentFormat::R16_S_Float));
}

void ComputeTemporalVariance(rg::RenderGraphBuilder& graphBuilder, const TemporalVarianceParams& params)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(params.outputVarianceTexture.IsValid());

	const math::Vector2u resolution = params.outputVarianceTexture->GetResolution2D();

	lib::MTHandle<SRComputeVarianceDS> ds = graphBuilder.CreateDescriptorSet<SRComputeVarianceDS>(RENDERER_RESOURCE_NAME("SR Compute Std Dev DS"));
	ds->u_rwVarianceTexture             = params.outputVarianceTexture;
	ds->u_accumulatedSamplesNumTexture  = params.accumulatedSamplesNumTexture;
	ds->u_depthTexture                  = params.depthTexture;
	ds->u_momentsTexture                = params.momentsTexture;
	ds->u_normalsTexture                = params.normalsTexture;
	ds->u_luminanceTexture              = params.luminanceTexture;

	static const rdr::PipelineStateID pipeline = CreateComputeVariancePipeline();

	graphBuilder.Dispatch(RG_DEBUG_NAME_FORMATTED("{} SR Compute Variance", params.debugName.AsString()),
						  pipeline,
						  math::Utils::DivideCeil(resolution, math::Vector2u(8u, 4u)),
						  rg::BindDescriptorSets(std::move(ds), params.renderView.GetRenderViewDS()));
}

} // spt::rsc::sr_denoiser
