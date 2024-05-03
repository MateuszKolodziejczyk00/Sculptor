#include "ShadingTexturesDownsampler.h"
#include "RenderGraphBuilder.h"
#include "RGDescriptorSetState.h"
#include "DescriptorSetBindings/RWTextureBinding.h"
#include "DescriptorSetBindings/SRVTextureBinding.h"
#include "DescriptorSetBindings/SamplerBinding.h"
#include "ResourcesManager.h"
#include "View/RenderView.h"


namespace spt::rsc
{

DS_BEGIN(DownsampleShadingTexturesDS, rg::RGDescriptorSetState<DownsampleShadingTexturesDS>)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),                                    u_depthTexture)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::NearestClampToEdge>), u_nearestSampler)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),                                    u_roughnessTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector4f>),                            u_tangentFrame)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<Real32>),                                     u_roughnessTextureHalfRes)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<math::Vector3f>),                             u_shadingNormalsTextureHalfRes)
DS_END();


static rdr::PipelineStateID CompileDownsampleShadingTexturesPipeline()
{
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/Utils/DownsampleShadingTextures.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "DownsampleShadingTexturesCS"));
	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("DownsampleShadingTexturesPipeline"), shader);
}


void DownsampleShadingTextures(rg::RenderGraphBuilder& graphBuilder, const RenderView& renderView, const DownsampledShadingTexturesParams& params)
{
	SPT_PROFILER_FUNCTION();

	const math::Vector2u halfRes = params.shadingNormalsHalfRes->GetResolution2D();

	lib::MTHandle<DownsampleShadingTexturesDS> ds = graphBuilder.CreateDescriptorSet<DownsampleShadingTexturesDS>(RENDERER_RESOURCE_NAME("DownsampleShadingTexturesDS"));
	ds->u_depthTexture                 = params.depth;
	ds->u_roughnessTexture             = params.roughness;
	ds->u_tangentFrame                 = params.tangentFrame;
	ds->u_roughnessTextureHalfRes      = params.roughnessHalfRes;
	ds->u_shadingNormalsTextureHalfRes = params.shadingNormalsHalfRes;

	const rdr::PipelineStateID pipeline = CompileDownsampleShadingTexturesPipeline();

	graphBuilder.Dispatch(RG_DEBUG_NAME("Downsample Shading Textures"),
						  pipeline,
						  math::Utils::DivideCeil(halfRes, math::Vector2u(8u, 8u)),
						  rg::BindDescriptorSets(std::move(ds), renderView.GetRenderViewDS()));
}

} // spt::rsc
