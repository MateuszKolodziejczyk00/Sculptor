#include "SRDisocclussionFix.h"
#include "RenderGraphBuilder.h"
#include "ShaderStructs/ShaderStructs.h"
#include "ResourcesManager.h"
#include "SRDenoiserTypes.h"


namespace spt::rsc::sr_denoiser
{

BEGIN_SHADER_STRUCT(SRDisocclusionFixConstants)
	SHADER_STRUCT_FIELD(math::Vector2u,                          resolution)
	SHADER_STRUCT_FIELD(math::Vector2f,                          pixelSize)
	SHADER_STRUCT_FIELD(Int32,                                   filterStride)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<Uint32>,               specularHistoryLengthTexture)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<Uint32>,               diffuseHistoryLengthTexture)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<Real32>,               depthTexture)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<Real32>,               roughnessTexture)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<math::Vector2f>,       normalsTexture)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<RTSphericalBasisType>, inSpecularY_SH2)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<RTSphericalBasisType>, inDiffuseY_SH2)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<math::Vector4f>,       inDiffSpecCoCg)
	SHADER_STRUCT_FIELD(gfx::UAVTexture2D<RTSphericalBasisType>, outSpecularY_SH2)
	SHADER_STRUCT_FIELD(gfx::UAVTexture2D<RTSphericalBasisType>, outDiffuseY_SH2)
	SHADER_STRUCT_FIELD(gfx::UAVTexture2D<math::Vector4f>,       outDiffSpecCoCg)
END_SHADER_STRUCT();


static rdr::PipelineStateID CreateDisocclusionFixPipeline()
{
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/SpecularReflections/Denoiser/SRDisocclusionFix.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "SRDisocclusionFixCS"));
	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("Specular Reflections Disocclusion Fix Pipeline"), shader);
}


void DisocclusionFix(rg::RenderGraphBuilder& graphBuilder, const DisocclusionFixParams& params)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(params.inSpecularY_SH2.IsValid());
	SPT_CHECK(params.inDiffuseY_SH2.IsValid());
	SPT_CHECK(params.inDiffSpecCoCg.IsValid());
	SPT_CHECK(params.outSpecularY_SH2.IsValid());
	SPT_CHECK(params.outDiffuseY_SH2.IsValid());
	SPT_CHECK(params.outDiffSpecCoCg.IsValid());

	const math::Vector2u resolution = params.outDiffuseY_SH2->GetResolution2D();

	SRDisocclusionFixConstants shaderConstants;
	shaderConstants.resolution   = resolution;
	shaderConstants.pixelSize    = resolution.cast<Real32>().cwiseInverse();
	shaderConstants.filterStride = 4u;
	shaderConstants.specularHistoryLengthTexture = params.specularHistoryLengthTexture;
	shaderConstants.diffuseHistoryLengthTexture  = params.diffuseHistoryLengthTexture;
	shaderConstants.depthTexture                 = params.depthTexture;
	shaderConstants.roughnessTexture             = params.roughnessTexture;
	shaderConstants.normalsTexture               = params.normalsTexture;
	shaderConstants.inSpecularY_SH2              = params.inSpecularY_SH2;
	shaderConstants.inDiffuseY_SH2               = params.inDiffuseY_SH2;
	shaderConstants.inDiffSpecCoCg               = params.inDiffSpecCoCg;
	shaderConstants.outSpecularY_SH2             = params.outSpecularY_SH2;
	shaderConstants.outDiffuseY_SH2              = params.outDiffuseY_SH2;
	shaderConstants.outDiffSpecCoCg              = params.outDiffSpecCoCg;

	static const rdr::PipelineStateID pipeline = CreateDisocclusionFixPipeline();

	graphBuilder.Dispatch(RG_DEBUG_NAME_FORMATTED("{} SR Disocclusion Fix", params.debugName.AsString()),
						  pipeline,
						  math::Utils::DivideCeil(resolution, math::Vector2u(8u, 8u)),
						  rg::ShaderParams(shaderConstants));
}

} // spt::rsc::sr_denoiser
