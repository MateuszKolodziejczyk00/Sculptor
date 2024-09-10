#include "DepthBasedUpsampler.h"
#include "RGDescriptorSetState.h"
#include "ResourcesManager.h"
#include "RenderGraphBuilder.h"
#include "DescriptorSetBindings/SRVTextureBinding.h"
#include "DescriptorSetBindings/SamplerBinding.h"
#include "DescriptorSetBindings/RWTextureBinding.h"
#include "MathUtils.h"
#include "View/RenderView.h"


namespace spt::rsc
{

namespace upsampler
{

BEGIN_SHADER_STRUCT(DepthBasedUpsampleConstants)
	SHADER_STRUCT_FIELD(Uint32, fireflyFilteringEnabled)
END_SHADER_STRUCT();


DS_BEGIN(DepthBasedUpsampleDS, rg::RGDescriptorSetState<DepthBasedUpsampleDS>)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),                                    u_depthTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),                                    u_depthTextureHalfRes)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector2f>),                            u_normalsTextureHalfRes)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector4f>),                            u_inputTexture)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::NearestClampToEdge>), u_nearestSampler)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<math::Vector4f>),                             u_outputTexture)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<DepthBasedUpsampleConstants>),             u_constants)
DS_END()


static rdr::PipelineStateID CompileDepthBasedUpsamplePipeline()
{
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/Utils/DepthBasedUpsample.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "DepthBasedUpsampleCS"));
	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("DepthBasedUpsamplePipeline"), shader);
}


rg::RGTextureViewHandle DepthBasedUpsample(rg::RenderGraphBuilder& graphBuilder, rg::RGTextureViewHandle texture, const DepthBasedUpsampleParams& params)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(params.depth.IsValid());
	SPT_CHECK(params.depthHalfRes.IsValid());
	SPT_CHECK(params.normalsHalfRes.IsValid());
	SPT_CHECK(params.renderViewDS.IsValid());
	SPT_CHECK(texture.IsValid());

	const math::Vector2u inputResolution = params.depthHalfRes->GetResolution2D();
	const math::Vector2u outputResolution = params.depth->GetResolution2D();

	SPT_CHECK(texture->GetResolution2D() == inputResolution);

	rg::TextureDef outputTextureDef = texture->GetTextureDefinition();
	outputTextureDef.resolution = outputResolution;

	const rg::RGTextureViewHandle outputTexture = graphBuilder.CreateTextureView(RG_DEBUG_NAME(std::format("{} (Upsampled)", texture->GetName().ToString())), outputTextureDef);

	DepthBasedUpsampleConstants constants;
	constants.fireflyFilteringEnabled = params.fireflyFilteringEnabled ? 1u : 0u;

	lib::MTHandle<DepthBasedUpsampleDS> descriptorSet = graphBuilder.CreateDescriptorSet<DepthBasedUpsampleDS>(RENDERER_RESOURCE_NAME("DepthBasedUpsampleDS"));
	descriptorSet->u_depthTexture          = params.depth;
	descriptorSet->u_depthTextureHalfRes   = params.depthHalfRes;
	descriptorSet->u_normalsTextureHalfRes = params.normalsHalfRes;
	descriptorSet->u_inputTexture          = texture;
	descriptorSet->u_outputTexture         = outputTexture;
	descriptorSet->u_constants             = constants;

	static const rdr::PipelineStateID pipeline = CompileDepthBasedUpsamplePipeline();

	graphBuilder.Dispatch(RG_DEBUG_NAME(std::format("Depth Based Upsample: {}", params.debugName.AsString())),
						  pipeline,
						  math::Utils::DivideCeil(outputResolution, math::Vector2u(8u, 8u)),
						  rg::BindDescriptorSets(std::move(descriptorSet), params.renderViewDS));
	
	return outputTexture;
}

} // upsampler

} // spt::rsc
