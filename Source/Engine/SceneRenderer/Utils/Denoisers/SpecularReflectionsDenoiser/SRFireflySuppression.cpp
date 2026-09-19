#include "SRFireflySuppression.h"
#include "RenderGraphBuilder.h"
#include "ShaderStructs/ShaderStructs.h"
#include "ResourcesManager.h"
#include "View/RenderView.h"
#include "SRDenoiserTypes.h"


namespace spt::rsc::sr_denoiser
{

BEGIN_SHADER_STRUCT(SRFireflySuppressionConstants)
	SHADER_STRUCT_FIELD(math::Vector2u,                          resolution)
	SHADER_STRUCT_FIELD(gfx::UAVTexture2D<RTSphericalBasisType>, outDiffuseY_SH2)
	SHADER_STRUCT_FIELD(gfx::UAVTexture2D<RTSphericalBasisType>, outSpecularY_SH2)
	SHADER_STRUCT_FIELD(gfx::UAVTexture2D<math::Vector4f>,       outDiffSpecCoCg)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<RTSphericalBasisType>, inDiffuseY_SH2)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<RTSphericalBasisType>, inSpecularY_SH2)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<math::Vector4f>,       inDiffSpecCoCg)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<math::Vector2f>,       normals)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<Real32>,               depth)
END_SHADER_STRUCT();


static rdr::PipelineStateID CreateFireflySuppressionPipeline()
{
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/SpecularReflections/Denoiser/SRFireflySuppression.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "SRFireflySuppressionCS"));
	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("Specular Reflections Firefly Suppression Pipeline"), shader);
}


void SuppressFireflies(rg::RenderGraphBuilder& graphBuilder, const FireflySuppressionParams& params)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(params.normal.IsValid());
	SPT_CHECK(params.depth.IsValid());
	SPT_CHECK(params.inSpecularY_SH2.IsValid());
	SPT_CHECK(params.inDiffuseY_SH2.IsValid());
	SPT_CHECK(params.inDiffSpecCoCg.IsValid());
	SPT_CHECK(params.outSpecularY_SH2.IsValid());
	SPT_CHECK(params.outDiffuseY_SH2.IsValid());
	SPT_CHECK(params.outDiffSpecCoCg.IsValid());

	const math::Vector2u resolution = params.inSpecularY_SH2->GetResolution2D();

	SRFireflySuppressionConstants shaderConstants;
	shaderConstants.resolution       = resolution;
	shaderConstants.outDiffuseY_SH2  = params.outDiffuseY_SH2;
	shaderConstants.outSpecularY_SH2 = params.outSpecularY_SH2;
	shaderConstants.outDiffSpecCoCg  = params.outDiffSpecCoCg;
	shaderConstants.inDiffuseY_SH2   = params.inDiffuseY_SH2;
	shaderConstants.inSpecularY_SH2  = params.inSpecularY_SH2;
	shaderConstants.inDiffSpecCoCg   = params.inDiffSpecCoCg;
	shaderConstants.normals          = params.normal;
	shaderConstants.depth            = params.depth;

	static const rdr::PipelineStateID pipeline = CreateFireflySuppressionPipeline();

	graphBuilder.Dispatch(RG_DEBUG_NAME_FORMATTED("{} SR Firefly Suppression", params.debugName.AsString()),
						  pipeline,
						  math::Utils::DivideCeil(resolution, math::Vector2u(8u, 8u)),
						  rg::ShaderParams(shaderConstants));
								
}

} // spt::rsc::sr_denoiser
