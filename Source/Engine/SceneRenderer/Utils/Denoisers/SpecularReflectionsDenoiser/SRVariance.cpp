#include "SRVariance.h"
#include "RenderGraphBuilder.h"
#include "ShaderStructs/ShaderStructs.h"
#include "ResourcesManager.h"
#include "Utils/Denoisers/SpecularReflectionsDenoiser/SRDenoiserTypes.h"
#include "View/RenderView.h"


namespace spt::rsc::sr_denoiser
{

BEGIN_SHADER_STRUCT(SRComputeVarianceConstants)
	SHADER_STRUCT_FIELD(math::Vector2u,                          resolution)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<math::Vector2f>,       specularMomentsTexture)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<math::Vector2f>,       diffuseMomentsTexture)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<RTSphericalBasisType>, specularY_SH2)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<RTSphericalBasisType>, diffuseY_SH2)
	SHADER_STRUCT_FIELD(gfx::UAVTexture2D<math::Vector2f>,       rwVarianceTexture)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<Uint32>,               specularHistoryLengthTexture)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<Uint32>,               diffuseHistoryLengthTexture)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<Real32>,               depthTexture)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<math::Vector2f>,       normalsTexture)
END_SHADER_STRUCT();


static rdr::PipelineStateID CreateComputeVariancePipeline()
{
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/SpecularReflections/Denoiser/SRComputeVariance.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "SRComputeVarianceCS"));
	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("Specular Reflections Compute Variance Pipeline"), shader);
}

rg::RGTextureViewHandle CreateVarianceTexture(rg::RenderGraphBuilder& graphBuilder, const rg::RenderGraphDebugName& name, math::Vector2u resolution)
{
	return graphBuilder.CreateTextureView(name, rg::TextureDef(resolution, rhi::EFragmentFormat::RG16_S_Float));
}

void ComputeTemporalVariance(rg::RenderGraphBuilder& graphBuilder, const TemporalVarianceParams& params)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(params.specularMomentsTexture.IsValid());
	SPT_CHECK(params.specularY_SH2.IsValid());
	
	SPT_CHECK(params.diffuseMomentsTexture.IsValid());
	SPT_CHECK(params.diffuseY_SH2.IsValid());

	SPT_CHECK(params.outVarianceTexture.IsValid());

	const math::Vector2u resolution = params.outVarianceTexture->GetResolution2D();

	SRComputeVarianceConstants shaderConstants;
	shaderConstants.resolution                   = resolution;
	shaderConstants.specularMomentsTexture       = params.specularMomentsTexture;
	shaderConstants.diffuseMomentsTexture        = params.diffuseMomentsTexture;
	shaderConstants.specularY_SH2                = params.specularY_SH2;
	shaderConstants.diffuseY_SH2                 = params.diffuseY_SH2;
	shaderConstants.rwVarianceTexture            = params.outVarianceTexture;
	shaderConstants.specularHistoryLengthTexture = params.specularHistoryLengthTexture;
	shaderConstants.diffuseHistoryLengthTexture  = params.diffuseHistoryLengthTexture;
	shaderConstants.depthTexture                 = params.depthTexture;
	shaderConstants.normalsTexture               = params.normalsTexture;

	static const rdr::PipelineStateID pipeline = CreateComputeVariancePipeline();

	graphBuilder.Dispatch(RG_DEBUG_NAME_FORMATTED("{} SR Compute Variance", params.debugName.AsString()),
						  pipeline,
						  math::Utils::DivideCeil(resolution, math::Vector2u(8u, 4u)),
						  rg::ShaderParams(shaderConstants));
}


BEGIN_SHADER_STRUCT(SREstimateVarianceConstants)
	SHADER_STRUCT_FIELD(math::Vector2u,                    resolution)
	SHADER_STRUCT_FIELD(math::Vector2f,                    invResolution)
	SHADER_STRUCT_FIELD(math::Vector4f,                    weights)
	SHADER_STRUCT_FIELD(gfx::UAVTexture2D<math::Vector2f>, rwVarianceEstimationTexture)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<math::Vector2f>, inVarianceTexture)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<Uint32>,         specularHistoryLengthTexture)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<Uint32>,         diffuseHistoryLengthTexture)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<Real32>,         depthTexture)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<math::Vector2f>, normalsTexture)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<Real32>,         roughnessTexture)
END_SHADER_STRUCT();


static rdr::PipelineStateID CreateEstimateVariancePipeline(Bool isHorizontal)
{
	sc::ShaderCompilationSettings compilationSettings;
	compilationSettings.AddMacroDefinition(sc::MacroDefinition("HORIZONTAL_PASS", isHorizontal));
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/SpecularReflections/Denoiser/SREstimateVariance.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "SREstimateVarianceCS"), compilationSettings);
	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("Specular Reflections Estimate Variance Pipeline"), shader);
}


void EstimateVariance(rg::RenderGraphBuilder& graphBuilder, const VarianceEstimationParams& params)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(params.inOutVarianceTexture.IsValid());

	const math::Vector2u resolution = params.inOutVarianceTexture->GetResolution2D();

	math::Array4f weights;
	math::Utils::ComputeGaussianBlurWeights<Real32>(weights.data(), static_cast<Uint32>(weights.size()), params.gaussianBlurSigma);

	SREstimateVarianceConstants shaderConstants;
	shaderConstants.resolution    = resolution;
	shaderConstants.invResolution = resolution.cast<Real32>().cwiseInverse();
	shaderConstants.weights       = weights;

	{
		static const rdr::PipelineStateID horizontalPipeline = CreateEstimateVariancePipeline(true);

		shaderConstants.rwVarianceEstimationTexture  = params.intermediateVarianceTexture;
		shaderConstants.inVarianceTexture            = params.inOutVarianceTexture;
		shaderConstants.specularHistoryLengthTexture = params.specularHistoryLengthTexture;
		shaderConstants.diffuseHistoryLengthTexture  = params.diffuseHistoryLengthTexture;
		shaderConstants.depthTexture                 = params.depthTexture;
		shaderConstants.normalsTexture               = params.normalsTexture;
		shaderConstants.roughnessTexture             = params.roughnessTexture;

		graphBuilder.Dispatch(RG_DEBUG_NAME_FORMATTED("{} SR Estimate Variance (Horizontal Pass)", params.debugName.AsString()),
							  horizontalPipeline,
							  math::Utils::DivideCeil(resolution, math::Vector2u(16u, 16u)),
							  rg::ShaderParams(shaderConstants));
	}

	{
		static const rdr::PipelineStateID verticalPipeline = CreateEstimateVariancePipeline(false);

		shaderConstants.rwVarianceEstimationTexture  = params.inOutVarianceTexture;
		shaderConstants.inVarianceTexture            = params.intermediateVarianceTexture;
		shaderConstants.specularHistoryLengthTexture = params.specularHistoryLengthTexture;
		shaderConstants.diffuseHistoryLengthTexture  = params.diffuseHistoryLengthTexture;
		shaderConstants.depthTexture                 = params.depthTexture;
		shaderConstants.normalsTexture               = params.normalsTexture;
		shaderConstants.roughnessTexture             = params.roughnessTexture;

		graphBuilder.Dispatch(RG_DEBUG_NAME_FORMATTED("{} SR Estimate Variance (Vertical Pass)", params.debugName.AsString()),
							  verticalPipeline,
							  math::Utils::DivideCeil(resolution, math::Vector2u(16u, 16u)),
							  rg::ShaderParams(shaderConstants));
	}
}

} // spt::rsc::sr_denoiser
