#include "SRATrousFilter.h"
#include "ShaderStructs/ShaderStructs.h"
#include "DescriptorSetBindings/SRVTextureBinding.h"
#include "DescriptorSetBindings/RWTextureBinding.h"
#include "DescriptorSetBindings/ConstantBufferBinding.h"
#include "DescriptorSetBindings/SamplerBinding.h"
#include "RGDescriptorSetState.h"
#include "ResourcesManager.h"
#include "RenderGraphBuilder.h"
#include "View/RenderView.h"
#include "Bindless/BindlessTypes.h"
#include "SRDenoiserTypes.h"


namespace spt::rsc::sr_denoiser
{

BEGIN_SHADER_STRUCT(SRATrousFilteringParams)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2DRef<RTSphericalBasisType>, inSpecularY)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2DRef<RTSphericalBasisType>, inDiffuseY)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2DRef<math::Vector4f>,       inDiffSpecCoCg)

	SHADER_STRUCT_FIELD(gfx::UAVTexture2D<RTSphericalBasisType>, rwSpecularY)
	SHADER_STRUCT_FIELD(gfx::UAVTexture2D<RTSphericalBasisType>, rwDiffuseY)
	SHADER_STRUCT_FIELD(gfx::UAVTexture2D<math::Vector4f>, rwDiffSpecCoCg)

	SHADER_STRUCT_FIELD(gfx::UAVTexture2D<math::Vector3f>, rwSpecularRGB)
	SHADER_STRUCT_FIELD(gfx::UAVTexture2D<math::Vector3f>, rwDiffuseRGB)

	SHADER_STRUCT_FIELD(math::Vector2u, resolution)
	SHADER_STRUCT_FIELD(math::Vector2f, invResolution)
	SHADER_STRUCT_FIELD(Int32,          samplesOffset)
END_SHADER_STRUCT();


DS_BEGIN(SRATrousFilterDS, rg::RGDescriptorSetState<SRATrousFilterDS>)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector2f>),            u_inVariance)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<math::Vector2f>),             u_outVariance)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),                    u_linearDepthTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector2f>),            u_normalsTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),                    u_roughnessTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Uint32>),                    u_specularHistoryLengthTexture)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<SRATrousFilteringParams>), u_constants)
DS_END();


static rdr::PipelineStateID CreateSRATrousFilterPipeline(Bool wideRadius, Bool outputSH)
{
	sc::ShaderCompilationSettings compilationSettings;
	compilationSettings.AddMacroDefinition(sc::MacroDefinition("WIDE_RADIUS", wideRadius ? "1" : "0"));
	compilationSettings.AddMacroDefinition(sc::MacroDefinition("OUTPUT_SH", outputSH ? "1" : "0"));
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/SpecularReflections/Denoiser/SRATrousFilter.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "SRATrousFilterCS"), compilationSettings);
	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("SR A-Trous Filter Pipeline"), shader);
}


void ApplyATrousFilter(rg::RenderGraphBuilder& graphBuilder, const SRATrousFilterParams& filterParams, const SRATrousPass& passParams)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(passParams.inSHTextures);
	SPT_CHECK(passParams.outTextures || passParams.outSHTextures);

	SPT_CHECK(passParams.inSHTextures->diffuseY_SH2.IsValid());
	SPT_CHECK(passParams.inSHTextures->specularY_SH2.IsValid());
	SPT_CHECK(passParams.inSHTextures->diffSpecCoCg.IsValid());

	if (passParams.outTextures)
	{
		SPT_CHECK(passParams.outTextures->specular.IsValid());
		SPT_CHECK(passParams.outTextures->diffuse.IsValid());
	}
	else
	{
		SPT_CHECK(passParams.outSHTextures->diffuseY_SH2.IsValid());
		SPT_CHECK(passParams.outSHTextures->specularY_SH2.IsValid());
		SPT_CHECK(passParams.outSHTextures->diffSpecCoCg.IsValid());
	}
	SPT_CHECK(passParams.inVariance.IsValid());
	SPT_CHECK(passParams.outVariance.IsValid());

	const math::Vector2u resolution = passParams.inSHTextures->diffuseY_SH2->GetResolution2D();

	const Bool outputSH = passParams.outSHTextures != nullptr;

	const rdr::PipelineStateID pipeline = CreateSRATrousFilterPipeline(passParams.enableWideFilter, outputSH);

	SRATrousFilteringParams dispatchConstants;
	dispatchConstants.samplesOffset = 1u << passParams.iterationIdx;
	dispatchConstants.resolution    = resolution;
	dispatchConstants.invResolution = resolution.cast<Real32>().cwiseInverse();

	dispatchConstants.inSpecularY    = passParams.inSHTextures->specularY_SH2;
	dispatchConstants.inDiffuseY     = passParams.inSHTextures->diffuseY_SH2;
	dispatchConstants.inDiffSpecCoCg = passParams.inSHTextures->diffSpecCoCg;

	if (passParams.outTextures)
	{
		dispatchConstants.rwSpecularRGB = passParams.outTextures->specular;
		dispatchConstants.rwDiffuseRGB  = passParams.outTextures->diffuse;
	}
	else
	{
		dispatchConstants.rwSpecularY    = passParams.outSHTextures->specularY_SH2;
		dispatchConstants.rwDiffuseY     = passParams.outSHTextures->diffuseY_SH2;
		dispatchConstants.rwDiffSpecCoCg = passParams.outSHTextures->diffSpecCoCg;
	}

	lib::MTHandle<SRATrousFilterDS> ds = graphBuilder.CreateDescriptorSet<SRATrousFilterDS>(RENDERER_RESOURCE_NAME("SR A-Trous Filter DS"));
	ds->u_inVariance                    = passParams.inVariance;
	ds->u_outVariance                   = passParams.outVariance;
	ds->u_linearDepthTexture            = filterParams.linearDepthTexture;
	ds->u_normalsTexture                = filterParams.normalsTexture;
	ds->u_roughnessTexture              = filterParams.roughnessTexture;
	ds->u_specularHistoryLengthTexture  = filterParams.specularHistoryLengthTexture;
	ds->u_constants                     = dispatchConstants;

	graphBuilder.Dispatch(RG_DEBUG_NAME(std::format("{}: Denoise Spatial A-Trous Filter (Iteration {})", filterParams.name.Get().ToString(), passParams.iterationIdx)),
						  pipeline,
						  math::Utils::DivideCeil(resolution, math::Vector2u(8u, 8u)),
						  rg::BindDescriptorSets(std::move(ds)));
}

} // spt::rsc::sr_denoiser
