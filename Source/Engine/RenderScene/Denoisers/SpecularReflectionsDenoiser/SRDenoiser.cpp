#include "SRDenoiser.h"
#include "Types/Texture.h"
#include "View/ViewRenderingSpec.h"
#include "RenderGraphBuilder.h"
#include "ResourcesManager.h"
#include "SRTemporalAccumulation.h"
#include "SRATrousFilter.h"
#include "SRVariance.h"
#include "SRDisocclussionFix.h"
#include "SRClampHistory.h"
#include "SRFireflySuppression.h"
#include "ShaderStructs.h"
#include "Bindless/BindlessTypes.h"
#include "RGDescriptorSetState.h"
#include "DescriptorSetBindings/ConstantBufferBinding.h"


namespace spt::rsc::sr_denoiser
{

struct SHTextures
{
	rg::RGTextureViewHandle specularY;
	rg::RGTextureViewHandle diffuseY;
	rg::RGTextureViewHandle diffSpecCoCg;
};


namespace utils
{

BEGIN_SHADER_STRUCT(RTPackToSHConstants)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<math::Vector2f>, normals)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<math::Vector3f>, specular)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<math::Vector3f>, diffuse)
	SHADER_STRUCT_FIELD(gfx::UAVTexture2D<math::Vector4f>, rwDiffuseY)
	SHADER_STRUCT_FIELD(gfx::UAVTexture2D<math::Vector4f>, rwSpecularY)
	SHADER_STRUCT_FIELD(gfx::UAVTexture2D<math::Vector4f>, rwDiffSpecCoCg)
END_SHADER_STRUCT();


DS_BEGIN(RTPackToSHDS, rg::RGDescriptorSetState<RTPackToSHDS>)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<RTPackToSHConstants>), u_constants)
DS_END();


static rdr::PipelineStateID CreateRTPackToSHPipeline()
{
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/SpecularReflections/Denoiser/RTPackToSH.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "RTPackToSHCS"));
	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("RT Pack To SH Pipeline"), shader);
}

static void PackToSH(rg::RenderGraphBuilder& graphBuilder, rg::RGTextureViewHandle spec, rg::RGTextureViewHandle diff, rg::RGTextureViewHandle normals, const SHTextures& sh)
{
	SPT_PROFILER_FUNCTION();

	RTPackToSHConstants shaderConstants;
	shaderConstants.normals        = normals;
	shaderConstants.specular       = spec;
	shaderConstants.diffuse        = diff;
	shaderConstants.rwDiffuseY     = sh.diffuseY;
	shaderConstants.rwSpecularY    = sh.specularY;
	shaderConstants.rwDiffSpecCoCg = sh.diffSpecCoCg;

	lib::MTHandle<RTPackToSHDS> ds = graphBuilder.CreateDescriptorSet<RTPackToSHDS>(RENDERER_RESOURCE_NAME("RT Pack To SH DS"));
	ds->u_constants = shaderConstants;

	static const rdr::PipelineStateID pipeline = CreateRTPackToSHPipeline();

	graphBuilder.Dispatch(RG_DEBUG_NAME_FORMATTED("RT Pack To SH"),
						  pipeline,
						  math::Utils::DivideCeil(spec->GetResolution2D(), math::Vector2u(8u, 8u)),
						  rg::BindDescriptorSets(std::move(ds)));
}

} // utils

Denoiser::Denoiser(rg::RenderGraphDebugName debugName)
	: m_debugName(debugName)
{ }

Denoiser::Result Denoiser::Denoise(rg::RenderGraphBuilder& graphBuilder, const Params& params)
{
	SPT_PROFILER_FUNCTION();

	UpdateResources(graphBuilder, params);

	return DenoiseImpl(graphBuilder, params);
}

rg::RGTextureViewHandle Denoiser::GetHistorySpecularHitDist(rg::RenderGraphBuilder& graphBuilder) const
{
	return m_historySpecularHitDist ? graphBuilder.AcquireExternalTextureView(m_historySpecularHitDist) : rg::RGTextureViewHandle{};
}

rg::RGTextureViewHandle Denoiser::GetDiffuseHistoryLength(rg::RenderGraphBuilder& graphBuilder) const
{
	return m_diffuseHistoryLengthTexture ? graphBuilder.AcquireExternalTextureView(m_diffuseHistoryLengthTexture) : rg::RGTextureViewHandle{};
}

void Denoiser::UpdateResources(rg::RenderGraphBuilder& graphBuilder, const Params& params)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(!!params.specularTexture);
	SPT_CHECK(!!params.diffuseTexture);
	SPT_CHECK(params.specularTexture->GetResolution() == params.diffuseTexture->GetResolution());
	SPT_CHECK(params.specularTexture->GetFormat() == params.diffuseTexture->GetFormat());

	const math::Vector2u resolution = params.specularTexture->GetResolution2D();

	if (!m_historySpecularY_SH2 || m_historySpecularY_SH2->GetResolution2D() != resolution)
	{
		rhi::TextureDefinition historyTexturesDefinition;
		historyTexturesDefinition.resolution = resolution;
		historyTexturesDefinition.usage      = lib::Flags(rhi::ETextureUsage::SampledTexture, rhi::ETextureUsage::StorageTexture, rhi::ETextureUsage::TransferDest);
		historyTexturesDefinition.format     = params.specularTexture->GetFormat();
		m_historySpecularY_SH2 = rdr::ResourcesManager::CreateTextureView(RENDERER_RESOURCE_NAME("RT History Specular SH2"), historyTexturesDefinition, rhi::EMemoryUsage::GPUOnly);
		m_historyDiffuseY_SH2  = rdr::ResourcesManager::CreateTextureView(RENDERER_RESOURCE_NAME("RT History Diffuse SH2"), historyTexturesDefinition, rhi::EMemoryUsage::GPUOnly);
		m_historyDiffSpecCoCg  = rdr::ResourcesManager::CreateTextureView(RENDERER_RESOURCE_NAME("RT History DiffSpec CoCg"), historyTexturesDefinition, rhi::EMemoryUsage::GPUOnly);

		rhi::TextureDefinition historyHitDistTextureDef;
		historyHitDistTextureDef.resolution = resolution;
		historyHitDistTextureDef.usage = lib::Flags(rhi::ETextureUsage::SampledTexture, rhi::ETextureUsage::StorageTexture, rhi::ETextureUsage::TransferDest);
		historyHitDistTextureDef.format = rhi::EFragmentFormat::R16_S_Float;
		m_historySpecularHitDist = rdr::ResourcesManager::CreateTextureView(RENDERER_RESOURCE_NAME("RT History Specular Hit Dist"), historyHitDistTextureDef, rhi::EMemoryUsage::GPUOnly);

		m_fastHistorySpecularTexture       = rdr::ResourcesManager::CreateTextureView(RENDERER_RESOURCE_NAME("RT Reflections Specular Fast History"), historyTexturesDefinition, rhi::EMemoryUsage::GPUOnly);
		m_fastHistorySpecularOutputTexture = rdr::ResourcesManager::CreateTextureView(RENDERER_RESOURCE_NAME("RT Reflections Specular Fast History"), historyTexturesDefinition, rhi::EMemoryUsage::GPUOnly);

		m_fastHistoryDiffuseTexture       = rdr::ResourcesManager::CreateTextureView(RENDERER_RESOURCE_NAME("RT Reflections Diffuse Fast History"), historyTexturesDefinition, rhi::EMemoryUsage::GPUOnly);
		m_fastHistoryDiffuseOutputTexture = rdr::ResourcesManager::CreateTextureView(RENDERER_RESOURCE_NAME("RT Reflections Diffuse Fast History"), historyTexturesDefinition, rhi::EMemoryUsage::GPUOnly);

		rhi::TextureDefinition accumulatedSamplesNumTextureDef;
		accumulatedSamplesNumTextureDef.resolution = resolution;
		accumulatedSamplesNumTextureDef.usage      = lib::Flags(rhi::ETextureUsage::SampledTexture, rhi::ETextureUsage::StorageTexture);
		accumulatedSamplesNumTextureDef.format     = rhi::EFragmentFormat::R8_U_Int;
		m_specularHistoryLengthTexture        = rdr::ResourcesManager::CreateTextureView(RENDERER_RESOURCE_NAME("RT Specular Reflections History Lenght"), accumulatedSamplesNumTextureDef, rhi::EMemoryUsage::GPUOnly);
		m_historySpecularHistoryLengthTexture = rdr::ResourcesManager::CreateTextureView(RENDERER_RESOURCE_NAME("RT Specular Reflections History Lenght"), accumulatedSamplesNumTextureDef, rhi::EMemoryUsage::GPUOnly);
		m_diffuseHistoryLengthTexture         = rdr::ResourcesManager::CreateTextureView(RENDERER_RESOURCE_NAME("RT Diffuse Reflections History Lenght"), accumulatedSamplesNumTextureDef, rhi::EMemoryUsage::GPUOnly);
		m_historyDiffuseHistoryLengthTexture  = rdr::ResourcesManager::CreateTextureView(RENDERER_RESOURCE_NAME("RT Diffuse Reflections History Lenght"), accumulatedSamplesNumTextureDef, rhi::EMemoryUsage::GPUOnly);

		rhi::TextureDefinition temporalVarianceTextureDef;
		temporalVarianceTextureDef.resolution = resolution;
		temporalVarianceTextureDef.usage      = lib::Flags(rhi::ETextureUsage::SampledTexture, rhi::ETextureUsage::StorageTexture, rhi::ETextureUsage::TransferDest);
		temporalVarianceTextureDef.format     = rhi::EFragmentFormat::RG16_S_Float;
		m_temporalVarianceSpecularTexture        = rdr::ResourcesManager::CreateTextureView(RENDERER_RESOURCE_NAME("RT Specular Reflections Temporal Variance"), temporalVarianceTextureDef, rhi::EMemoryUsage::GPUOnly);
		m_historyTemporalVarianceSpecularTexture = rdr::ResourcesManager::CreateTextureView(RENDERER_RESOURCE_NAME("RT Specular Reflections Temporal Variance"), temporalVarianceTextureDef, rhi::EMemoryUsage::GPUOnly);

		m_temporalVarianceDiffuseTexture        = rdr::ResourcesManager::CreateTextureView(RENDERER_RESOURCE_NAME("RT Diffuse Reflections Temporal Variance"), temporalVarianceTextureDef, rhi::EMemoryUsage::GPUOnly);
		m_historyTemporalVarianceDiffuseTexture = rdr::ResourcesManager::CreateTextureView(RENDERER_RESOURCE_NAME("RT Diffuse Reflections Temporal Variance"), temporalVarianceTextureDef, rhi::EMemoryUsage::GPUOnly);

		m_hasValidHistory = false;
	}
}

Denoiser::Result Denoiser::DenoiseImpl(rg::RenderGraphBuilder& graphBuilder, const Params& params)
{
	SPT_PROFILER_FUNCTION();

	SPT_RG_DIAGNOSTICS_SCOPE(graphBuilder, "RT Denoiser");

	const RenderView& renderView = params.viewSpec.GetRenderView();
	const ShadingViewContext& viewContext = params.viewSpec.GetShadingViewContext();

	const rg::RGTextureViewHandle specularTexture = params.specularTexture;
	const rg::RGTextureViewHandle diffuseTexture  = params.diffuseTexture;

	const rg::RGTextureViewHandle specularY_SH2 = graphBuilder.CreateTextureView(RG_DEBUG_NAME("Specular Y SH2"), rg::TextureDef(specularTexture->GetResolution2D(), rhi::EFragmentFormat::RGBA16_S_Float));
	const rg::RGTextureViewHandle diffuseY_SH2  = graphBuilder.CreateTextureView(RG_DEBUG_NAME("Diffuse Y SH2"), rg::TextureDef(diffuseTexture->GetResolution2D(), rhi::EFragmentFormat::RGBA16_S_Float));
	const rg::RGTextureViewHandle diffSpecCoCg  = graphBuilder.CreateTextureView(RG_DEBUG_NAME("Diffuse Specular CoCg"), rg::TextureDef(diffuseTexture->GetResolution2D(), rhi::EFragmentFormat::RGBA16_S_Float));
	const rg::RGTextureViewHandle specHitDist   = graphBuilder.AcquireExternalTextureView(m_historySpecularHitDist);

	SPT_CHECK(specularTexture.IsValid());
	SPT_CHECK(diffuseTexture.IsValid());

	const math::Vector2u resolution = specularTexture->GetResolution2D();

	const rg::RGTextureViewHandle historySpecularY_SH2 = graphBuilder.AcquireExternalTextureView(lib::Ref(m_historySpecularY_SH2));
	const rg::RGTextureViewHandle historyDiffuseY_SH2  = graphBuilder.AcquireExternalTextureView(lib::Ref(m_historyDiffuseY_SH2));
	const rg::RGTextureViewHandle historyDiffSpecCoCg  = graphBuilder.AcquireExternalTextureView(lib::Ref(m_historyDiffSpecCoCg));

	const rg::RGTextureViewHandle specularHistoryLengthTexture           = graphBuilder.AcquireExternalTextureView(lib::Ref(m_specularHistoryLengthTexture));
	const rg::RGTextureViewHandle historySpecularHistoryLengthTexture    = graphBuilder.AcquireExternalTextureView(lib::Ref(m_historySpecularHistoryLengthTexture));
	const rg::RGTextureViewHandle diffuseHistoryLengthTexture            = graphBuilder.AcquireExternalTextureView(lib::Ref(m_diffuseHistoryLengthTexture));
	const rg::RGTextureViewHandle historyDiffuseHistoryLengthTexture     = graphBuilder.AcquireExternalTextureView(lib::Ref(m_historyDiffuseHistoryLengthTexture));
	const rg::RGTextureViewHandle fastHistorySpecularTexture             = graphBuilder.AcquireExternalTextureView(lib::Ref(m_fastHistorySpecularTexture));
	const rg::RGTextureViewHandle fastHistorySpecularOutputTexture       = graphBuilder.AcquireExternalTextureView(lib::Ref(m_fastHistorySpecularOutputTexture));
	const rg::RGTextureViewHandle fastHistoryDiffuseTexture              = graphBuilder.AcquireExternalTextureView(lib::Ref(m_fastHistoryDiffuseTexture));
	const rg::RGTextureViewHandle fastHistoryDiffuseOutputTexture        = graphBuilder.AcquireExternalTextureView(lib::Ref(m_fastHistoryDiffuseOutputTexture));
	const rg::RGTextureViewHandle temporalVarianceSpecularTexture        = graphBuilder.AcquireExternalTextureView(lib::Ref(m_temporalVarianceSpecularTexture));
	const rg::RGTextureViewHandle historyTemporalVarianceSpecularTexture = graphBuilder.AcquireExternalTextureView(lib::Ref(m_historyTemporalVarianceSpecularTexture));
	const rg::RGTextureViewHandle temporalVarianceDiffuseTexture         = graphBuilder.AcquireExternalTextureView(lib::Ref(m_temporalVarianceDiffuseTexture));
	const rg::RGTextureViewHandle historyTemporalVarianceDiffuseTexture  = graphBuilder.AcquireExternalTextureView(lib::Ref(m_historyTemporalVarianceDiffuseTexture));

	if (m_hasValidHistory && !params.resetAccumulation)
	{
		SPT_CHECK(params.historyNormalsTexture.IsValid());
		SPT_CHECK(params.historyDepthTexture.IsValid());
		SPT_CHECK(params.historyRoughnessTexture.IsValid());

		TemporalAccumulationParameters temporalAccumulationParameters(renderView);
		temporalAccumulationParameters.name                                   = m_debugName;
		temporalAccumulationParameters.historyDepthTexture                    = params.historyDepthTexture;
		temporalAccumulationParameters.currentDepthTexture                    = params.currentDepthTexture;
		temporalAccumulationParameters.currentRoughnessTexture                = params.roughnessTexture;
		temporalAccumulationParameters.motionTexture                          = params.motionTexture;
		temporalAccumulationParameters.normalsTexture                         = params.normalsTexture;
		temporalAccumulationParameters.historyNormalsTexture                  = params.historyNormalsTexture;
		temporalAccumulationParameters.currentSpecularTexture                 = specularTexture;
		temporalAccumulationParameters.currentDiffuseTexture                  = diffuseTexture;
		temporalAccumulationParameters.specularY_SH2                          = specularY_SH2;
		temporalAccumulationParameters.diffuseY_SH2                           = diffuseY_SH2;
		temporalAccumulationParameters.diffSpecCoCg                           = diffSpecCoCg;
		temporalAccumulationParameters.specHitDist                            = specHitDist;
		temporalAccumulationParameters.historySpecularY_SH2                   = historySpecularY_SH2;
		temporalAccumulationParameters.historyDiffuseY_SH2                    = historyDiffuseY_SH2;
		temporalAccumulationParameters.historyDiffSpecCoCg                    = historyDiffSpecCoCg;
		temporalAccumulationParameters.specularHistoryLengthTexture           = specularHistoryLengthTexture;
		temporalAccumulationParameters.historySpecularHistoryLengthTexture    = historySpecularHistoryLengthTexture;
		temporalAccumulationParameters.diffuseHistoryLengthTexture            = diffuseHistoryLengthTexture;
		temporalAccumulationParameters.historyDiffuseHistoryLengthTexture     = historyDiffuseHistoryLengthTexture;
		temporalAccumulationParameters.temporalVarianceSpecularTexture        = temporalVarianceSpecularTexture;
		temporalAccumulationParameters.historyTemporalVarianceSpecularTexture = historyTemporalVarianceSpecularTexture;
		temporalAccumulationParameters.temporalVarianceDiffuseTexture         = temporalVarianceDiffuseTexture;
		temporalAccumulationParameters.historyTemporalVarianceDiffuseTexture  = historyTemporalVarianceDiffuseTexture;
		temporalAccumulationParameters.historyRoughnessTexture                = params.historyRoughnessTexture;
		temporalAccumulationParameters.fastHistorySpecularTexture             = fastHistorySpecularTexture;
		temporalAccumulationParameters.fastHistorySpecularOutputTexture       = fastHistorySpecularOutputTexture;
		temporalAccumulationParameters.fastHistoryDiffuseTexture              = fastHistoryDiffuseTexture;
		temporalAccumulationParameters.fastHistoryDiffuseOutputTexture        = fastHistoryDiffuseOutputTexture;
		temporalAccumulationParameters.enableStableHistoryBlend               = params.enableStableHistoryBlend;
		temporalAccumulationParameters.enableDisocclusionFixFromLightCache    = params.enableDisocclusionFixFromLightCache;
		temporalAccumulationParameters.baseColorMetallic                      = params.baseColorMetallicTexture;
		temporalAccumulationParameters.sharcCacheDS                           = viewContext.sharcCacheDS;

		ApplyTemporalAccumulation(graphBuilder, temporalAccumulationParameters);
	}
	else
	{
		graphBuilder.CopyFullTexture(RG_DEBUG_NAME_FORMATTED("{}: Copy Specular Fast History", m_debugName.AsString()), specularTexture, fastHistorySpecularOutputTexture);
		graphBuilder.CopyFullTexture(RG_DEBUG_NAME_FORMATTED("{}: Copy Diffuse Fast History", m_debugName.AsString()), diffuseTexture, fastHistoryDiffuseOutputTexture);
		graphBuilder.ClearTexture(RG_DEBUG_NAME_FORMATTED("{}: Clear Specular Moments", m_debugName.AsString()), temporalVarianceSpecularTexture, rhi::ClearColor(0.f, 0.f, 0.f, 0.f));
		graphBuilder.ClearTexture(RG_DEBUG_NAME_FORMATTED("{}: Clear Diffuse Moments", m_debugName.AsString()), temporalVarianceDiffuseTexture, rhi::ClearColor(0.f, 0.f, 0.f, 0.f));

		utils::PackToSH(graphBuilder, specularTexture, diffuseTexture, params.normalsTexture, SHTextures{specularY_SH2, diffuseY_SH2, diffSpecCoCg});
	}

	const rg::RGTextureViewHandle outSpecularTexture = graphBuilder.CreateTextureView(RG_DEBUG_NAME_FORMATTED("{}: Output Specular Texture", m_debugName.AsString()), rg::TextureDef(resolution, specularTexture->GetFormat()));
	const rg::RGTextureViewHandle outDiffuseTexture  = graphBuilder.CreateTextureView(RG_DEBUG_NAME_FORMATTED("{}: Output Diffuse Texture", m_debugName.AsString()), rg::TextureDef(resolution, diffuseTexture->GetFormat()));

	DisocclusionFixParams disocclusionFixParams(renderView);
	disocclusionFixParams.debugName                    = m_debugName;
	disocclusionFixParams.specularHistoryLengthTexture = specularHistoryLengthTexture;
	disocclusionFixParams.diffuseHistoryLengthTexture  = diffuseHistoryLengthTexture;
	disocclusionFixParams.normalsTexture               = params.normalsTexture;
	disocclusionFixParams.depthTexture                 = params.currentDepthTexture;
	disocclusionFixParams.roughnessTexture             = params.roughnessTexture;
	disocclusionFixParams.inSpecularY_SH2              = specularY_SH2;
	disocclusionFixParams.inDiffuseY_SH2               = diffuseY_SH2;
	disocclusionFixParams.inDiffSpecCoCg               = diffSpecCoCg;
	disocclusionFixParams.outSpecularY_SH2             = historySpecularY_SH2;
	disocclusionFixParams.outDiffuseY_SH2              = historyDiffuseY_SH2;
	disocclusionFixParams.outDiffSpecCoCg              = historyDiffSpecCoCg;
	DisocclusionFix(graphBuilder, disocclusionFixParams);

	ClampHistoryParams clampHistoryParams(renderView);
	clampHistoryParams.debugName                      = m_debugName;
	clampHistoryParams.specularHistoryLengthTexture   = specularHistoryLengthTexture;
	clampHistoryParams.diffuseHistoryLengthTexture    = diffuseHistoryLengthTexture;
	clampHistoryParams.depthTexture                   = params.currentDepthTexture;
	clampHistoryParams.normalsTexture                 = params.normalsTexture;
	clampHistoryParams.roughnessTexture               = params.roughnessTexture;
	clampHistoryParams.fastHistorySpecularTexture     = fastHistorySpecularTexture;
	clampHistoryParams.fastHistoryDiffuseTexture      =	fastHistoryDiffuseTexture;
	clampHistoryParams.specularY_SH2                  = historySpecularY_SH2;
	clampHistoryParams.diffuseY_SH2                   = historyDiffuseY_SH2;
	clampHistoryParams.diffSpecCoCg                   = historyDiffSpecCoCg;
	clampHistoryParams.diffuseHistoryLenght           = diffuseHistoryLengthTexture;
	clampHistoryParams.specularHistoryLenght          = specularHistoryLengthTexture;
	ClampHistory(graphBuilder, clampHistoryParams);

	FireflySuppressionParams fireflySuppressionParams;
	fireflySuppressionParams.debugName        = m_debugName;
	fireflySuppressionParams.normal           = params.normalsTexture;
	fireflySuppressionParams.inSpecularY_SH2  = historySpecularY_SH2;
	fireflySuppressionParams.inDiffuseY_SH2   = historyDiffuseY_SH2;
	fireflySuppressionParams.inDiffSpecCoCg   = historyDiffSpecCoCg;
	fireflySuppressionParams.outSpecularY_SH2 = specularY_SH2;
	fireflySuppressionParams.outDiffuseY_SH2  = diffuseY_SH2;
	fireflySuppressionParams.outDiffSpecCoCg  = diffSpecCoCg;
	SuppressFireflies(graphBuilder, fireflySuppressionParams);

	const rg::RGTextureViewHandle temporalVariance     = CreateVarianceTexture(graphBuilder, RG_DEBUG_NAME("Temporal Variance"), resolution);
	const rg::RGTextureViewHandle intermediateVariance = CreateVarianceTexture(graphBuilder, RG_DEBUG_NAME("Intermediate Variance"), resolution);

	TemporalVarianceParams temporalVarianceParams(renderView);
	temporalVarianceParams.debugName                    = m_debugName;
	temporalVarianceParams.specularHistoryLengthTexture = specularHistoryLengthTexture;
	temporalVarianceParams.diffuseHistoryLengthTexture  = diffuseHistoryLengthTexture;
	temporalVarianceParams.normalsTexture               = params.normalsTexture;
	temporalVarianceParams.depthTexture                 = params.currentDepthTexture;
	temporalVarianceParams.specularMomentsTexture       = temporalVarianceSpecularTexture;
	temporalVarianceParams.diffuseMomentsTexture        = temporalVarianceDiffuseTexture;
	temporalVarianceParams.specularY_SH2                = specularY_SH2;
	temporalVarianceParams.diffuseY_SH2                 = diffuseY_SH2;
	temporalVarianceParams.outVarianceTexture           = temporalVariance;
	ComputeTemporalVariance(graphBuilder, temporalVarianceParams);

	const rg::RGTextureViewHandle varianceEstimation = CreateVarianceTexture(graphBuilder, RG_DEBUG_NAME("Variance Estimation"), resolution);

	SpatialFilterParams spatialParams;
	spatialParams.inSpecularY_SH2              = specularY_SH2;
	spatialParams.inDiffuseY_SH2               = diffuseY_SH2;
	spatialParams.inDiffSpecCoCg               = diffSpecCoCg;
	spatialParams.outSpecular                  = outSpecularTexture;
	spatialParams.outDiffuse                   = outDiffuseTexture;
	spatialParams.inVariance                   = temporalVariance;
	spatialParams.intermediateVariance         = intermediateVariance;
	spatialParams.outVarianceEstimation        = varianceEstimation;
	spatialParams.specularHistoryLengthTexture = specularHistoryLengthTexture;

	ApplySpatialFilter(graphBuilder, spatialParams, params);

	if (params.blurVarianceEstimate)
	{
		VarianceEstimationParams varianceEstimationParams(renderView);
		varianceEstimationParams.debugName                    = m_debugName;
		varianceEstimationParams.specularHistoryLengthTexture = specularHistoryLengthTexture;
		varianceEstimationParams.diffuseHistoryLengthTexture  = diffuseHistoryLengthTexture;
		varianceEstimationParams.normalsTexture               = params.normalsTexture;
		varianceEstimationParams.depthTexture                 = params.currentDepthTexture;
		varianceEstimationParams.roughnessTexture             = params.currentDepthTexture;
		varianceEstimationParams.inOutVarianceTexture         = varianceEstimation;
		varianceEstimationParams.intermediateVarianceTexture  = intermediateVariance;
		EstimateVariance(graphBuilder, varianceEstimationParams);
	}

	Result result;
	result.denoisedSpecular   = outSpecularTexture;
	result.denoisedDiffuse    = outDiffuseTexture;
	result.varianceEstimation = varianceEstimation;

	std::swap(m_specularHistoryLengthTexture, m_historySpecularHistoryLengthTexture);
	std::swap(m_diffuseHistoryLengthTexture, m_historyDiffuseHistoryLengthTexture);

	std::swap(m_fastHistorySpecularTexture, m_fastHistorySpecularOutputTexture);
	std::swap(m_fastHistoryDiffuseTexture, m_fastHistoryDiffuseOutputTexture);

	std::swap(m_temporalVarianceSpecularTexture, m_historyTemporalVarianceSpecularTexture);
	std::swap(m_temporalVarianceDiffuseTexture, m_historyTemporalVarianceDiffuseTexture);

	m_hasValidHistory = true;

	return result;
}

void Denoiser::ApplySpatialFilter(rg::RenderGraphBuilder& graphBuilder, const SpatialFilterParams& spatialParams, const Params& params)
{
	SPT_PROFILER_FUNCTION();

	SPT_RG_DIAGNOSTICS_SCOPE(graphBuilder, "RT Denoiser: Spatial Filters");

	const math::Vector2u resolution = spatialParams.inSpecularY_SH2->GetResolution2D();

	const rg::RGTextureViewHandle intermediateDiffuseY_SH2  = graphBuilder.CreateTextureView(RG_DEBUG_NAME("Intermediate Diffuse Y SH2"), rg::TextureDef(resolution, rhi::EFragmentFormat::RGBA16_S_Float));
	const rg::RGTextureViewHandle intermediateSpecularY_SH2 = graphBuilder.CreateTextureView(RG_DEBUG_NAME("Intermediate Specular Y SH2"), rg::TextureDef(resolution, rhi::EFragmentFormat::RGBA16_S_Float));
	const rg::RGTextureViewHandle intermediateDiffSpecCoCg  = graphBuilder.CreateTextureView(RG_DEBUG_NAME("Intermediate Diffuse CoCg"), rg::TextureDef(resolution, rhi::EFragmentFormat::RGBA16_S_Float));

	SRATrousFilterParams aTrousParams(params.viewSpec.GetRenderView());
	aTrousParams.name                         = m_debugName;
	aTrousParams.linearDepthTexture           = params.linearDepthTexture;
	aTrousParams.depthTexture                 = params.currentDepthTexture;
	aTrousParams.normalsTexture               = params.normalsTexture;
	aTrousParams.roughnessTexture             = params.roughnessTexture;
	aTrousParams.specularHistoryLengthTexture = spatialParams.specularHistoryLengthTexture;

	const auto advanceIteration = [&params](SRATrousPass& passParams, rg::RGTextureViewHandle inputVariance, rg::RGTextureViewHandle outputVariance)
	{
		std::swap(passParams.inSHTextures, passParams.outSHTextures);

		passParams.inVariance       = inputVariance;
		passParams.outVariance      = outputVariance;
		passParams.enableWideFilter = passParams.iterationIdx < params.wideRadiusPassesNum;

		++passParams.iterationIdx;
	};

	const SRATrousPass::SHTextures passSHTextures0{ spatialParams.inDiffuseY_SH2, spatialParams.inSpecularY_SH2, spatialParams.inDiffSpecCoCg };
	const SRATrousPass::SHTextures passSHTextures1{ intermediateDiffuseY_SH2, intermediateSpecularY_SH2, intermediateDiffSpecCoCg };

	const rg::RGTextureViewHandle historySpecularY_SH2 = graphBuilder.AcquireExternalTextureView(lib::Ref(m_historySpecularY_SH2));
	const rg::RGTextureViewHandle historyDiffuseY_SH2  = graphBuilder.AcquireExternalTextureView(lib::Ref(m_historyDiffuseY_SH2));
	const rg::RGTextureViewHandle historyDiffSpecCoCg  = graphBuilder.AcquireExternalTextureView(lib::Ref(m_historyDiffSpecCoCg));

	const SRATrousPass::SHTextures passSHTexturesHistory{ historyDiffuseY_SH2, historySpecularY_SH2, historyDiffSpecCoCg };

	SRATrousPass pass;
	pass.inSHTextures     = &passSHTextures0;
	pass.outSHTextures    = &passSHTextures1;
	//pass.outSHTextures    = &passSHTexturesHistory;
	pass.inVariance       = spatialParams.inVariance;
	pass.outVariance      = spatialParams.outVarianceEstimation;
	pass.enableWideFilter = params.wideRadiusPassesNum > 0;

	ApplyATrousFilter(graphBuilder, aTrousParams, pass);

	advanceIteration(pass, spatialParams.outVarianceEstimation, spatialParams.intermediateVariance);
	ApplyATrousFilter(graphBuilder, aTrousParams, pass);

	advanceIteration(pass, spatialParams.intermediateVariance, spatialParams.inVariance);
	//pass.outSHTextures    = &passSHTextures1;
	ApplyATrousFilter(graphBuilder, aTrousParams, pass);

	advanceIteration(pass, spatialParams.inVariance, spatialParams.intermediateVariance);
	ApplyATrousFilter(graphBuilder, aTrousParams, pass);

	const SRATrousPass::Textures finalTextures
	{
         .specular = spatialParams.outSpecular,
         .diffuse = spatialParams.outDiffuse
	};

	advanceIteration(pass, spatialParams.intermediateVariance, spatialParams.inVariance);
	pass.outSHTextures = nullptr;
	pass.outTextures   = &finalTextures;
	ApplyATrousFilter(graphBuilder, aTrousParams, pass);
}

} // spt::rsc::sr_denoise
