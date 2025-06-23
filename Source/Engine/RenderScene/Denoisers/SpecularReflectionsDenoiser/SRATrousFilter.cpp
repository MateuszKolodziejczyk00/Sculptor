#include "SRATrousFilter.h"
#include "ShaderStructs/ShaderStructsMacros.h"
#include "DescriptorSetBindings/SRVTextureBinding.h"
#include "DescriptorSetBindings/RWTextureBinding.h"
#include "DescriptorSetBindings/ConstantBufferBinding.h"
#include "DescriptorSetBindings/SamplerBinding.h"
#include "RGDescriptorSetState.h"
#include "ResourcesManager.h"
#include "RenderGraphBuilder.h"
#include "View/RenderView.h"


namespace spt::rsc::sr_denoiser
{

BEGIN_SHADER_STRUCT(SRATrousFilteringParams)
	SHADER_STRUCT_FIELD(math::Vector2u, resolution)
	SHADER_STRUCT_FIELD(math::Vector2f, invResolution)
	SHADER_STRUCT_FIELD(Int32,          samplesOffset)
END_SHADER_STRUCT();


DS_BEGIN(SRATrousFilterDS, rg::RGDescriptorSetState<SRATrousFilterDS>)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector4f>),            u_inSpecularLuminance)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector4f>),            u_inDiffuseLuminance)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<math::Vector4f>),             u_outSpecularLuminance)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<math::Vector4f>),             u_outDiffuseLuminance)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector2f>),            u_inVariance)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<math::Vector2f>),             u_outVariance)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),                    u_linearDepthTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector2f>),            u_normalsTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),                    u_roughnessTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Uint32>),                    u_specularHistoryLengthTexture)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<SRATrousFilteringParams>), u_params)
DS_END();


static rdr::PipelineStateID CreateSRATrousFilterPipeline(Bool wideRadius)
{
	sc::ShaderCompilationSettings compilationSettings;
	compilationSettings.AddMacroDefinition(sc::MacroDefinition("WIDE_RADIUS", wideRadius ? "1" : "0"));
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/SpecularReflections/Denoiser/SRATrousFilter.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "SRATrousFilterCS"), compilationSettings);
	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("SR A-Trous Filter Pipeline"), shader);
}


void ApplyATrousFilter(rg::RenderGraphBuilder& graphBuilder, const SRATrousFilterParams& filterParams, const SRATrousPass& passParams)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(passParams.inSpecularLuminance.IsValid());
	SPT_CHECK(passParams.inDiffuseLuminance.IsValid());
	SPT_CHECK(passParams.inVariance.IsValid());
	SPT_CHECK(passParams.outSpecularLuminance.IsValid());
	SPT_CHECK(passParams.outDiffuseLuminance.IsValid());
	SPT_CHECK(passParams.outVariance.IsValid());

	const math::Vector2u resolution = passParams.inSpecularLuminance->GetResolution2D();

	const rdr::PipelineStateID pipeline = CreateSRATrousFilterPipeline(passParams.enableWideFilter);

	SRATrousFilteringParams dispatchParams;
	dispatchParams.samplesOffset            = 1u << passParams.iterationIdx;
	dispatchParams.resolution               = resolution;
	dispatchParams.invResolution            = resolution.cast<Real32>().cwiseInverse();

	lib::MTHandle<SRATrousFilterDS> ds = graphBuilder.CreateDescriptorSet<SRATrousFilterDS>(RENDERER_RESOURCE_NAME("SR A-Trous Filter DS"));
	ds->u_inSpecularLuminance           = passParams.inSpecularLuminance;
	ds->u_inDiffuseLuminance            = passParams.inDiffuseLuminance;
	ds->u_outSpecularLuminance          = passParams.outSpecularLuminance;
	ds->u_outDiffuseLuminance           = passParams.outDiffuseLuminance;
	ds->u_inVariance                    = passParams.inVariance;
	ds->u_outVariance                   = passParams.outVariance;
	ds->u_linearDepthTexture            = filterParams.linearDepthTexture;
	ds->u_normalsTexture                = filterParams.normalsTexture;
	ds->u_roughnessTexture              = filterParams.roughnessTexture;
	ds->u_specularHistoryLengthTexture  = filterParams.specularHistoryLengthTexture;
	ds->u_params                        = dispatchParams;

	graphBuilder.Dispatch(RG_DEBUG_NAME(std::format("{}: Denoise Spatial A-Trous Filter (Iteration {})", filterParams.name.Get().ToString(), passParams.iterationIdx)),
						  pipeline,
						  math::Utils::DivideCeil(resolution, math::Vector2u(8u, 8u)),
						  rg::BindDescriptorSets(std::move(ds), filterParams.renderView.GetRenderViewDS()));
}

} // spt::rsc::sr_denoiser
