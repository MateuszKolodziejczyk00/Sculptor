#include "SRClampHistory.h"
#include "RenderGraphBuilder.h"
#include "ShaderStructs/ShaderStructs.h"
#include "ResourcesManager.h"
#include "View/RenderView.h"
#include "SRDenoiserTypes.h"


namespace spt::rsc::sr_denoiser
{

BEGIN_SHADER_STRUCT(SRClampHistoryConstants)
	SHADER_STRUCT_FIELD(math::Vector2u,                          resolution)
	SHADER_STRUCT_FIELD(math::Vector2f,                          pixelSize)
	SHADER_STRUCT_FIELD(gfx::UAVTexture2D<RTSphericalBasisType>, specularY_SH2)
	SHADER_STRUCT_FIELD(gfx::UAVTexture2D<RTSphericalBasisType>, diffuseY_SH2)
	SHADER_STRUCT_FIELD(gfx::UAVTexture2D<math::Vector4f>,       diffSpecCoCg)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<math::Vector3f>,       fastHistorySpecularTexture)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<math::Vector3f>,       fastHistoryDiffuseTexture)
	SHADER_STRUCT_FIELD(gfx::UAVTexture2D<Uint32>,               specularHistoryLengthTexture)
	SHADER_STRUCT_FIELD(gfx::UAVTexture2D<Uint32>,               diffuseHistoryLengthTexture)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<Real32>,               depthTexture)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<math::Vector2f>,       normalsTexture)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<Real32>,               roughnessTexture)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<Uint32>,               diffuseHistoryLength)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<Uint32>,               specularHistoryLength)
END_SHADER_STRUCT();


static rdr::PipelineStateID CreateClampHistoryPipeline()
{
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/SpecularReflections/Denoiser/SRClampHistory.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "SRClampHistoryCS"));
	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("Specular Reflections Clamp History Pipeline"), shader);
}


void ClampHistory(rg::RenderGraphBuilder& graphBuilder, const ClampHistoryParams& params)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(params.fastHistorySpecularTexture.IsValid());
	SPT_CHECK(params.specularY_SH2.IsValid());
	SPT_CHECK(params.diffuseY_SH2.IsValid());
	SPT_CHECK(params.diffSpecCoCg.IsValid());
	SPT_CHECK(params.fastHistoryDiffuseTexture.IsValid());

	const math::Vector2u resolution = params.specularY_SH2->GetResolution2D();

	SRClampHistoryConstants shaderConstants;
	shaderConstants.resolution = resolution;
	shaderConstants.pixelSize  = resolution.cast<Real32>().cwiseInverse();
	shaderConstants.specularY_SH2                = params.specularY_SH2;
	shaderConstants.diffuseY_SH2                 = params.diffuseY_SH2;
	shaderConstants.diffSpecCoCg                 = params.diffSpecCoCg;
	shaderConstants.fastHistorySpecularTexture   = params.fastHistorySpecularTexture;
	shaderConstants.fastHistoryDiffuseTexture    = params.fastHistoryDiffuseTexture;
	shaderConstants.specularHistoryLengthTexture = params.specularHistoryLengthTexture;
	shaderConstants.diffuseHistoryLengthTexture  = params.diffuseHistoryLengthTexture;
	shaderConstants.depthTexture                 = params.depthTexture;
	shaderConstants.normalsTexture               = params.normalsTexture;
	shaderConstants.roughnessTexture             = params.roughnessTexture;
	shaderConstants.diffuseHistoryLength         = params.diffuseHistoryLenght;
	shaderConstants.specularHistoryLength        = params.specularHistoryLenght;

	static const rdr::PipelineStateID pipeline = CreateClampHistoryPipeline();

	graphBuilder.Dispatch(RG_DEBUG_NAME_FORMATTED("{} SR Clamp History", params.debugName.AsString()),
						  pipeline,
						  math::Utils::DivideCeil(resolution, math::Vector2u(8u, 8u)),
						  rg::ShaderParams(shaderConstants));
}

} // spt::rsc::sr_denoiser
