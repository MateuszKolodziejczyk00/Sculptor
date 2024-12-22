#include "SRVariance.h"
#include "RenderGraphBuilder.h"
#include "RGDescriptorSetState.h"
#include "DescriptorSetBindings/RWTextureBinding.h"
#include "DescriptorSetBindings/SRVTextureBinding.h"
#include "DescriptorSetBindings/ConstantBufferBinding.h"
#include "ShaderStructs/ShaderStructsMacros.h"
#include "ResourcesManager.h"
#include "View/RenderView.h"


namespace spt::rsc::sr_denoiser
{

BEGIN_SHADER_STRUCT(SRComputeVarianceConstants)
	SHADER_STRUCT_FIELD(math::Vector2u, resolution)
END_SHADER_STRUCT();


DS_BEGIN(SRComputeVarianceDS, rg::RGDescriptorSetState<SRComputeVarianceDS>)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector2f>),               u_specularMomentsTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector3f>),               u_specularTexture)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<Real32>),                        u_rwSpecularVarianceTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector2f>),               u_diffuseMomentsTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector3f>),               u_diffuseTexture)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<Real32>),                        u_rwDiffuseVarianceTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Uint32>),                       u_accumulatedSamplesNumTexture)
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
	return graphBuilder.CreateTextureView(name, rg::TextureDef(resolution, rhi::EFragmentFormat::R16_S_Float));
}

void ComputeTemporalVariance(rg::RenderGraphBuilder& graphBuilder, const TemporalVarianceParams& params)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(params.specularMomentsTexture.IsValid());
	SPT_CHECK(params.specularTexture.IsValid());
	SPT_CHECK(params.outSpecularVarianceTexture.IsValid());
	
	SPT_CHECK(params.diffuseMomentsTexture.IsValid());
	SPT_CHECK(params.diffuseTexture.IsValid());
	SPT_CHECK(params.outDiffuseVarianceTexture.IsValid());

	const math::Vector2u resolution = params.outSpecularVarianceTexture->GetResolution2D();

	SRComputeVarianceConstants shaderConstants;
	shaderConstants.resolution = resolution;

	lib::MTHandle<SRComputeVarianceDS> ds = graphBuilder.CreateDescriptorSet<SRComputeVarianceDS>(RENDERER_RESOURCE_NAME("SR Compute Std Dev DS"));
	ds->u_specularMomentsTexture       = params.specularMomentsTexture;
	ds->u_specularTexture              = params.specularTexture;
	ds->u_rwSpecularVarianceTexture    = params.outSpecularVarianceTexture;
	ds->u_diffuseMomentsTexture        = params.diffuseMomentsTexture;
	ds->u_diffuseTexture               = params.diffuseTexture;
	ds->u_rwDiffuseVarianceTexture     = params.outDiffuseVarianceTexture;
	ds->u_accumulatedSamplesNumTexture = params.accumulatedSamplesNumTexture;
	ds->u_depthTexture                 = params.depthTexture;
	ds->u_normalsTexture               = params.normalsTexture;
	ds->u_constants                    = shaderConstants;

	static const rdr::PipelineStateID pipeline = CreateComputeVariancePipeline();

	graphBuilder.Dispatch(RG_DEBUG_NAME_FORMATTED("{} SR Compute Variance", params.debugName.AsString()),
						  pipeline,
						  math::Utils::DivideCeil(resolution, math::Vector2u(8u, 4u)),
						  rg::BindDescriptorSets(std::move(ds), params.renderView.GetRenderViewDS()));
}


BEGIN_SHADER_STRUCT(SREstimateVarianceConstants)
	SHADER_STRUCT_FIELD(math::Vector2u, resolution)
	SHADER_STRUCT_FIELD(math::Vector2f, invResolution)
	SHADER_STRUCT_FIELD(math::Vector4f, weights)
END_SHADER_STRUCT();


DS_BEGIN(SREstimateVarianceDS, rg::RGDescriptorSetState<SREstimateVarianceDS>)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<Real32>),                         u_rwVarianceEstimationTexture)
	DS_BINDING(BINDING_TYPE(gfx::OptionalSRVTexture2DBinding<Real32>),                u_varianceTexture)
	DS_BINDING(BINDING_TYPE(gfx::OptionalSRVTexture2DBinding<Real32>),                u_specularVarianceTexture)
	DS_BINDING(BINDING_TYPE(gfx::OptionalSRVTexture2DBinding<Real32>),                u_diffuseVvarianceTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Uint32>),                        u_accumulatedSamplesNumTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),                        u_depthTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector2f>),                u_normalsTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),                        u_roughnessTexture)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<SREstimateVarianceConstants>), u_constants)
DS_END();


static rdr::PipelineStateID CreateEstimateVariancePipeline(Bool isHorizontal, Bool mergeSpecularAndDiffuse)
{
	sc::ShaderCompilationSettings compilationSettings;
	compilationSettings.AddMacroDefinition(sc::MacroDefinition("HORIZONTAL_PASS", isHorizontal));
	compilationSettings.AddMacroDefinition(sc::MacroDefinition("MERGE_DIFFUSE_AND_SPECULAR", mergeSpecularAndDiffuse));
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/SpecularReflections/Denoiser/SREstimateVariance.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "SREstimateVarianceCS"), compilationSettings);
	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("Specular Reflections Estimate Variance Pipeline"), shader);
}


void EstimateVariance(rg::RenderGraphBuilder& graphBuilder, const VarianceEstimationParams& params)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(params.outEstimatedVarianceTexture.IsValid());

	const math::Vector2u resolution = params.outEstimatedVarianceTexture->GetResolution2D();

	math::Array4f weights;
	math::Utils::ComputeGaussianBlurWeights<Real32>(OUT { weights.data(), static_cast<SizeType>(weights.size()) }, params.gaussianBlurSigma);

	SREstimateVarianceConstants shaderConstants;
	shaderConstants.resolution    = resolution;
	shaderConstants.invResolution = resolution.cast<Real32>().cwiseInverse();
	shaderConstants.weights       = weights;

	{
		static const rdr::PipelineStateID horizontalPipeline = CreateEstimateVariancePipeline(true, true);

		const lib::MTHandle<SREstimateVarianceDS> ds = graphBuilder.CreateDescriptorSet<SREstimateVarianceDS>(RENDERER_RESOURCE_NAME("SR Estimate Variance DS"));
		ds->u_rwVarianceEstimationTexture  = params.intermediateVarianceTexture;
		ds->u_accumulatedSamplesNumTexture = params.accumulatedSamplesNumTexture;
		ds->u_specularVarianceTexture      = params.specularVarianceTexture;
		ds->u_diffuseVvarianceTexture      = params.diffuseVarianceTexture;
		ds->u_depthTexture                 = params.depthTexture;
		ds->u_normalsTexture               = params.normalsTexture;
		ds->u_roughnessTexture             = params.roughnessTexture;
		ds->u_constants                    = shaderConstants;

		graphBuilder.Dispatch(RG_DEBUG_NAME_FORMATTED("{} SR Estimate Variance (Horizontal Pass)", params.debugName.AsString()),
							  horizontalPipeline,
							  math::Utils::DivideCeil(resolution, math::Vector2u(16u, 16u)),
							  rg::BindDescriptorSets(std::move(ds), params.renderView.GetRenderViewDS()));
	}

	{
		static const rdr::PipelineStateID verticalPipeline = CreateEstimateVariancePipeline(false,false);

		const lib::MTHandle<SREstimateVarianceDS> ds = graphBuilder.CreateDescriptorSet<SREstimateVarianceDS>(RENDERER_RESOURCE_NAME("SR Estimate Variance DS"));
		ds->u_rwVarianceEstimationTexture  = params.outEstimatedVarianceTexture;
		ds->u_accumulatedSamplesNumTexture = params.accumulatedSamplesNumTexture;
		ds->u_varianceTexture              = params.intermediateVarianceTexture;
		ds->u_depthTexture                 = params.depthTexture;
		ds->u_normalsTexture               = params.normalsTexture;
		ds->u_roughnessTexture             = params.roughnessTexture;
		ds->u_constants                    = shaderConstants;

		graphBuilder.Dispatch(RG_DEBUG_NAME_FORMATTED("{} SR Estimate Variance (Vertical Pass)", params.debugName.AsString()),
							  verticalPipeline,
							  math::Utils::DivideCeil(resolution, math::Vector2u(16u, 16u)),
							  rg::BindDescriptorSets(std::move(ds), params.renderView.GetRenderViewDS()));
	}
}

} // spt::rsc::sr_denoiser
