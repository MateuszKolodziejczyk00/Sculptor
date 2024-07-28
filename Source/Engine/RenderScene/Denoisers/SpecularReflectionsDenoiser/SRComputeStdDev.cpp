#include "SRComputeStdDev.h"
#include "RenderGraphBuilder.h"
#include "RGDescriptorSetState.h"
#include "DescriptorSetBindings/RWTextureBinding.h"
#include "DescriptorSetBindings/SRVTextureBinding.h"
#include "ResourcesManager.h"
#include "View/RenderView.h"


namespace spt::rsc::sr_denoiser
{

DS_BEGIN(SRComputeStdDevDS, rg::RGDescriptorSetState<SRComputeStdDevDS>)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<Real32>),          u_stdDevTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Uint32>),         u_accumulatedSamplesNumTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),         u_depthTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector2f>), u_momentsTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector2f>), u_normalsTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector3f>), u_luminanceTexture)
DS_END();


static rdr::PipelineStateID CreateComputeStdDevPipeline()
{
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/SpecularReflections/Denoiser/SRComputeStdDev.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "SRComputeStdDevCS"));
	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("Specular Reflections Compute Std Dev Pipeline"), shader);
}


rg::RGTextureViewHandle ComputeStandardDeviation(rg::RenderGraphBuilder& graphBuilder, const StdDevParams& params)
{
	SPT_PROFILER_FUNCTION();

	const math::Vector2u resolution = params.momentsTexture->GetResolution2D();

	const rg::RGTextureViewHandle stdDevTexture = graphBuilder.CreateTextureView(RG_DEBUG_NAME("SR Std Dev"), rg::TextureDef(resolution, rhi::EFragmentFormat::R16_S_Float));

	lib::MTHandle<SRComputeStdDevDS> ds = graphBuilder.CreateDescriptorSet<SRComputeStdDevDS>(RENDERER_RESOURCE_NAME("SR Compute Std Dev DS"));
	ds->u_stdDevTexture                 = stdDevTexture;
	ds->u_accumulatedSamplesNumTexture  = params.accumulatedSamplesNumTexture;
	ds->u_depthTexture                  = params.depthTexture;
	ds->u_momentsTexture                = params.momentsTexture;
	ds->u_normalsTexture                = params.normalsTexture;
	ds->u_luminanceTexture              = params.luminanceTexture;

	static const rdr::PipelineStateID pipeline = CreateComputeStdDevPipeline();

	graphBuilder.Dispatch(RG_DEBUG_NAME_FORMATTED("{} SR Compute Std Dev", params.debugName.AsString()),
						  pipeline,
						  math::Utils::DivideCeil(resolution, math::Vector2u(8u, 4u)),
						  rg::BindDescriptorSets(std::move(ds), params.renderView.GetRenderViewDS()));

	return stdDevTexture;
}

} // spt::rsc::sr_denoiser
