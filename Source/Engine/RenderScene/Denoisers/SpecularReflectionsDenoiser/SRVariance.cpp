#include "SRVariance.h"
#include "RenderGraphBuilder.h"
#include "RGDescriptorSetState.h"
#include "DescriptorSetBindings/RWTextureBinding.h"
#include "DescriptorSetBindings/SRVTextureBinding.h"
#include "DescriptorSetBindings/ConstantBufferBinding.h"
#include "ShaderStructs/ShaderStructs.h"
#include "ResourcesManager.h"
#include "View/RenderView.h"


namespace spt::rsc::sr_denoiser
{

BEGIN_SHADER_STRUCT(SRComputeVarianceConstants)
	SHADER_STRUCT_FIELD(math::Vector2u, resolution)
END_SHADER_STRUCT();


DS_BEGIN(SRComputeVarianceDS, rg::RGDescriptorSetState<SRComputeVarianceDS>)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector2f>),               u_specularMomentsTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector2f>),               u_diffuseMomentsTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<RTSphericalBasisType>),         u_specularY_SH2)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<RTSphericalBasisType>),         u_diffuseY_SH2)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<math::Vector2f>),                u_rwVarianceTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Uint32>),                       u_specularHistoryLengthTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Uint32>),                       u_diffuseHistoryLengthTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),                       u_depthTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector2f>),               u_normalsTexture)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<SRComputeVarianceConstants>), u_constants)
DS_END();


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
	shaderConstants.resolution = resolution;

	lib::MTHandle<SRComputeVarianceDS> ds = graphBuilder.CreateDescriptorSet<SRComputeVarianceDS>(RENDERER_RESOURCE_NAME("SR Compute Std Dev DS"));
	ds->u_specularMomentsTexture       = params.specularMomentsTexture;
	ds->u_diffuseMomentsTexture        = params.diffuseMomentsTexture;
	ds->u_specularY_SH2                = params.specularY_SH2;
	ds->u_diffuseY_SH2                 = params.diffuseY_SH2;
	ds->u_rwVarianceTexture            = params.outVarianceTexture;
	ds->u_specularHistoryLengthTexture = params.specularHistoryLengthTexture;
	ds->u_diffuseHistoryLengthTexture  = params.diffuseHistoryLengthTexture;
	ds->u_depthTexture                 = params.depthTexture;
	ds->u_normalsTexture               = params.normalsTexture;
	ds->u_constants                    = shaderConstants;

	static const rdr::PipelineStateID pipeline = CreateComputeVariancePipeline();

	graphBuilder.Dispatch(RG_DEBUG_NAME_FORMATTED("{} SR Compute Variance", params.debugName.AsString()),
						  pipeline,
						  math::Utils::DivideCeil(resolution, math::Vector2u(8u, 4u)),
						  rg::BindDescriptorSets(std::move(ds)));
}


BEGIN_SHADER_STRUCT(SREstimateVarianceConstants)
	SHADER_STRUCT_FIELD(math::Vector2u, resolution)
	SHADER_STRUCT_FIELD(math::Vector2f, invResolution)
	SHADER_STRUCT_FIELD(math::Vector4f, weights)
END_SHADER_STRUCT();


DS_BEGIN(SREstimateVarianceDS, rg::RGDescriptorSetState<SREstimateVarianceDS>)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<math::Vector2f>),                 u_rwVarianceEstimationTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector2f>),                u_inVarianceTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Uint32>),                        u_specularHistoryLengthTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Uint32>),                        u_diffuseHistoryLengthTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),                        u_depthTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector2f>),                u_normalsTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),                        u_roughnessTexture)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<SREstimateVarianceConstants>), u_constants)
DS_END();


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
	math::Utils::ComputeGaussianBlurWeights<Real32>(OUT { weights.data(), static_cast<SizeType>(weights.size()) }, params.gaussianBlurSigma);

	SREstimateVarianceConstants shaderConstants;
	shaderConstants.resolution    = resolution;
	shaderConstants.invResolution = resolution.cast<Real32>().cwiseInverse();
	shaderConstants.weights       = weights;

	{
		static const rdr::PipelineStateID horizontalPipeline = CreateEstimateVariancePipeline(true);

		const lib::MTHandle<SREstimateVarianceDS> ds = graphBuilder.CreateDescriptorSet<SREstimateVarianceDS>(RENDERER_RESOURCE_NAME("SR Estimate Variance DS"));
		ds->u_rwVarianceEstimationTexture  = params.intermediateVarianceTexture;
		ds->u_inVarianceTexture            = params.inOutVarianceTexture;
		ds->u_specularHistoryLengthTexture = params.specularHistoryLengthTexture;
		ds->u_diffuseHistoryLengthTexture  = params.diffuseHistoryLengthTexture;
		ds->u_depthTexture                 = params.depthTexture;
		ds->u_normalsTexture               = params.normalsTexture;
		ds->u_roughnessTexture             = params.roughnessTexture;
		ds->u_constants                    = shaderConstants;

		graphBuilder.Dispatch(RG_DEBUG_NAME_FORMATTED("{} SR Estimate Variance (Horizontal Pass)", params.debugName.AsString()),
							  horizontalPipeline,
							  math::Utils::DivideCeil(resolution, math::Vector2u(16u, 16u)),
							  rg::BindDescriptorSets(std::move(ds)));
	}

	{
		static const rdr::PipelineStateID verticalPipeline = CreateEstimateVariancePipeline(false);

		const lib::MTHandle<SREstimateVarianceDS> ds = graphBuilder.CreateDescriptorSet<SREstimateVarianceDS>(RENDERER_RESOURCE_NAME("SR Estimate Variance DS"));
		ds->u_rwVarianceEstimationTexture  = params.inOutVarianceTexture;
		ds->u_inVarianceTexture            = params.intermediateVarianceTexture;
		ds->u_specularHistoryLengthTexture = params.specularHistoryLengthTexture;
		ds->u_diffuseHistoryLengthTexture  = params.diffuseHistoryLengthTexture;
		ds->u_depthTexture                 = params.depthTexture;
		ds->u_normalsTexture               = params.normalsTexture;
		ds->u_roughnessTexture             = params.roughnessTexture;
		ds->u_constants                    = shaderConstants;

		graphBuilder.Dispatch(RG_DEBUG_NAME_FORMATTED("{} SR Estimate Variance (Vertical Pass)", params.debugName.AsString()),
							  verticalPipeline,
							  math::Utils::DivideCeil(resolution, math::Vector2u(16u, 16u)),
							  rg::BindDescriptorSets(std::move(ds)));
	}
}

} // spt::rsc::sr_denoiser
