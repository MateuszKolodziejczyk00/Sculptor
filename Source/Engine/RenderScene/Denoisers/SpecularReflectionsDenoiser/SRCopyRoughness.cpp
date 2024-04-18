#include "SRCopyRoughness.h"
#include "RGDescriptorSetState.h"
#include "DescriptorSetBindings/RWTextureBinding.h"
#include "DescriptorSetBindings/SRVTextureBinding.h"
#include "DescriptorSetBindings/ConstantBufferBinding.h"
#include "ShaderStructs/ShaderStructsMacros.h"
#include "RenderGraphBuilder.h"


namespace spt::rsc::sr_denoiser
{

BEGIN_SHADER_STRUCT(SRCopyRoughnessConstants)
	SHADER_STRUCT_FIELD(math::Vector2u, resolution)
END_SHADER_STRUCT();


DS_BEGIN(SRCopyRoughnessDS, rg::RGDescriptorSetState<SRCopyRoughnessDS>)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<Real32>),                      u_destRoughnessTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector4f>),             u_specularColorAndRoughnessTexture)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<SRCopyRoughnessConstants>), u_constants)
DS_END();


static rdr::PipelineStateID CreateCopyRoughnessPipeline()
{
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/SpecularReflections/Denoiser/SRCopyRoughness.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "SRCopyRoughnessCS"));
	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("Specular Reflections Copy Roughness Pipeline"), shader);
}


void CopyRoughness(rg::RenderGraphBuilder& graphBuilder, rg::RGTextureViewHandle specularColorRoughnessTexture, rg::RGTextureViewHandle destRoughnessTexture)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(specularColorRoughnessTexture.IsValid());
	SPT_CHECK(destRoughnessTexture.IsValid());

	const math::Vector2u resolution = specularColorRoughnessTexture->GetResolution2D();

	SRCopyRoughnessConstants shaderConstants;
	shaderConstants.resolution = specularColorRoughnessTexture->GetResolution2D();

	lib::MTHandle<SRCopyRoughnessDS> ds = graphBuilder.CreateDescriptorSet<SRCopyRoughnessDS>(RENDERER_RESOURCE_NAME("SRCopyRoughnessDS"));
	ds->u_destRoughnessTexture             = destRoughnessTexture;
	ds->u_specularColorAndRoughnessTexture = specularColorRoughnessTexture;
	ds->u_constants                        = shaderConstants;

	static const rdr::PipelineStateID pipeline = CreateCopyRoughnessPipeline();

	graphBuilder.Dispatch(RG_DEBUG_NAME("SR Copy Roughness"),
						  pipeline,
						  math::Utils::DivideCeil(resolution, math::Vector2u(8u, 8u)),
						  rg::BindDescriptorSets(std::move(ds)));
}

} // spt::rsc::sr_denoiser
