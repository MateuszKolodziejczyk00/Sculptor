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

DS_BEGIN(SRComputeVarianceDS, rg::RGDescriptorSetState<SRComputeVarianceDS>)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<Real32>),          u_rwVarianceTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Uint32>),         u_accumulatedSamplesNumTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),         u_depthTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector2f>), u_momentsTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector2f>), u_normalsTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector3f>), u_luminanceTexture)
DS_END();


static rdr::PipelineStateID CreateComputeVariancePipeline()
{
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/SpecularReflections/Denoiser/SRComputeVariance.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "SRComputeVarianceCS"));
	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("Specular Reflections Compute Variance Pipeline"), shader);
}

rg::RGTextureViewHandle CreateVarianceTexture(rg::RenderGraphBuilder& graphBuilder, math::Vector2u resolution)
{
	return graphBuilder.CreateTextureView(RG_DEBUG_NAME("SR Variance"), rg::TextureDef(resolution, rhi::EFragmentFormat::R16_S_Float));
}

void ComputeTemporalVariance(rg::RenderGraphBuilder& graphBuilder, const TemporalVarianceParams& params)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(params.outputVarianceTexture.IsValid());

	const math::Vector2u resolution = params.outputVarianceTexture->GetResolution2D();

	lib::MTHandle<SRComputeVarianceDS> ds = graphBuilder.CreateDescriptorSet<SRComputeVarianceDS>(RENDERER_RESOURCE_NAME("SR Compute Std Dev DS"));
	ds->u_rwVarianceTexture             = params.outputVarianceTexture;
	ds->u_accumulatedSamplesNumTexture  = params.accumulatedSamplesNumTexture;
	ds->u_depthTexture                  = params.depthTexture;
	ds->u_momentsTexture                = params.momentsTexture;
	ds->u_normalsTexture                = params.normalsTexture;
	ds->u_luminanceTexture              = params.luminanceTexture;

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
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Uint32>),                        u_accumulatedSamplesNumTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),                        u_varianceTexture)
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

	SPT_CHECK(params.outputEstimatedVarianceTexture.IsValid());

	const math::Vector2u resolution = params.outputEstimatedVarianceTexture->GetResolution2D();

	math::Array4f weights;
	math::Utils::ComputeGaussianBlurWeights<Real32>(OUT { weights.data(), static_cast<SizeType>(weights.size()) }, params.gaussianBlurSigma);

	SREstimateVarianceConstants shaderConstants;
	shaderConstants.resolution    = resolution;
	shaderConstants.invResolution = resolution.cast<Real32>().cwiseInverse();
	shaderConstants.weights       = weights;

	{
		static const rdr::PipelineStateID horizontalPipeline = CreateEstimateVariancePipeline(true);

		const lib::MTHandle<SREstimateVarianceDS> ds = graphBuilder.CreateDescriptorSet<SREstimateVarianceDS>(RENDERER_RESOURCE_NAME("SR Estimate Variance DS"));
		ds->u_rwVarianceEstimationTexture  = params.tempVarianceTexture;
		ds->u_accumulatedSamplesNumTexture = params.accumulatedSamplesNumTexture;
		ds->u_varianceTexture              = params.varianceTexture;
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
		static const rdr::PipelineStateID verticalPipeline = CreateEstimateVariancePipeline(false);

		const lib::MTHandle<SREstimateVarianceDS> ds = graphBuilder.CreateDescriptorSet<SREstimateVarianceDS>(RENDERER_RESOURCE_NAME("SR Estimate Variance DS"));
		ds->u_rwVarianceEstimationTexture  = params.outputEstimatedVarianceTexture;
		ds->u_accumulatedSamplesNumTexture = params.accumulatedSamplesNumTexture;
		ds->u_varianceTexture              = params.tempVarianceTexture;
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
