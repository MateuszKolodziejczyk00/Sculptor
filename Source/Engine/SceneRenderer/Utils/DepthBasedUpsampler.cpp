#include "DepthBasedUpsampler.h"
#include "ResourcesManager.h"
#include "RenderGraphBuilder.h"
#include "MathUtils.h"
#include "View/RenderView.h"


namespace spt::rsc
{

namespace upsampler
{

BEGIN_SHADER_STRUCT(DepthBasedUpsampleConstants)
	SHADER_STRUCT_FIELD(Uint32,                            fireflyFilteringEnabled)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<Real32>,         depthTexture)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<Real32>,         depthTextureHalfRes)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<math::Vector2f>, normalsTextureHalfRes)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<math::Vector4f>, inputTexture)
	SHADER_STRUCT_FIELD(gfx::UAVTexture2D<math::Vector4f>, outputTexture)
END_SHADER_STRUCT();


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
	SPT_CHECK(texture.IsValid());

	const math::Vector2u inputResolution = params.depthHalfRes->GetResolution2D();
	const math::Vector2u outputResolution = params.depth->GetResolution2D();

	SPT_CHECK(texture->GetResolution2D() == inputResolution);

	rg::TextureDef outputTextureDef = texture->GetTextureDefinition();
	outputTextureDef.resolution = outputResolution;

	const rg::RGTextureViewHandle outputTexture = graphBuilder.CreateTextureView(RG_DEBUG_NAME(std::format("{} (Upsampled)", texture->GetName().ToString())), outputTextureDef);

	DepthBasedUpsampleConstants constants;
	constants.fireflyFilteringEnabled = params.fireflyFilteringEnabled ? 1u : 0u;
	constants.depthTexture            = params.depth;
	constants.depthTextureHalfRes     = params.depthHalfRes;
	constants.normalsTextureHalfRes   = params.normalsHalfRes;
	constants.inputTexture            = texture;
	constants.outputTexture           = outputTexture;

	static const rdr::PipelineStateID pipeline = CompileDepthBasedUpsamplePipeline();

	graphBuilder.Dispatch(RG_DEBUG_NAME(std::format("Depth Based Upsample: {}", params.debugName.AsString())),
						  pipeline,
						  math::Utils::DivideCeil(outputResolution, math::Vector2u(8u, 8u)),
						  rg::ShaderParams(constants));
	
	return outputTexture;
}

} // upsampler

} // spt::rsc
