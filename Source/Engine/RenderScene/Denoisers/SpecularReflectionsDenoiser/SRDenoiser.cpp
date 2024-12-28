#include "SRDenoiser.h"
#include "Types/Texture.h"
#include "RenderGraphBuilder.h"
#include "ResourcesManager.h"
#include "SRTemporalAccumulation.h"
#include "SRATrousFilter.h"
#include "SRVariance.h"
#include "SRDisocclussionFix.h"
#include "SRClampHistory.h"
#include "SRFireflySuppression.h"


namespace spt::rsc::sr_denoiser
{

Denoiser::Denoiser(rg::RenderGraphDebugName debugName)
	: m_debugName(debugName)
{ }

Denoiser::Result Denoiser::Denoise(rg::RenderGraphBuilder& graphBuilder, const Params& params)
{
	SPT_PROFILER_FUNCTION();

	UpdateResources(graphBuilder, params);

	return DenoiseImpl(graphBuilder, params);
}

void Denoiser::UpdateResources(rg::RenderGraphBuilder& graphBuilder, const Params& params)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(!!params.specularTexture);
	SPT_CHECK(!!params.diffuseTexture);
	SPT_CHECK(params.specularTexture->GetResolution() == params.diffuseTexture->GetResolution());
	SPT_CHECK(params.specularTexture->GetFormat() == params.diffuseTexture->GetFormat());

	const math::Vector2u resolution = params.specularTexture->GetResolution2D();

	if (!m_historySpecularTexture || m_historySpecularTexture->GetResolution2D() != resolution)
	{
		rhi::TextureDefinition historyTexturesDefinition;
		historyTexturesDefinition.resolution = resolution;
		historyTexturesDefinition.usage      = lib::Flags(rhi::ETextureUsage::SampledTexture, rhi::ETextureUsage::StorageTexture, rhi::ETextureUsage::TransferDest);
		historyTexturesDefinition.format     = params.specularTexture->GetFormat();
		m_historySpecularTexture = rdr::ResourcesManager::CreateTextureView(RENDERER_RESOURCE_NAME("RT Reflections Specular History"), historyTexturesDefinition, rhi::EMemoryUsage::GPUOnly);
		m_historyDiffuseTexture  = rdr::ResourcesManager::CreateTextureView(RENDERER_RESOURCE_NAME("RT Reflections Diffuse History"), historyTexturesDefinition, rhi::EMemoryUsage::GPUOnly);

		m_fastHistorySpecularTexture       = rdr::ResourcesManager::CreateTextureView(RENDERER_RESOURCE_NAME("RT Reflections Specular Fast History"), historyTexturesDefinition, rhi::EMemoryUsage::GPUOnly);
		m_fastHistorySpecularOutputTexture = rdr::ResourcesManager::CreateTextureView(RENDERER_RESOURCE_NAME("RT Reflections Specular Fast History"), historyTexturesDefinition, rhi::EMemoryUsage::GPUOnly);

		m_fastHistoryDiffuseTexture       = rdr::ResourcesManager::CreateTextureView(RENDERER_RESOURCE_NAME("RT Reflections Diffuse Fast History"), historyTexturesDefinition, rhi::EMemoryUsage::GPUOnly);
		m_fastHistoryDiffuseOutputTexture = rdr::ResourcesManager::CreateTextureView(RENDERER_RESOURCE_NAME("RT Reflections Diffuse Fast History"), historyTexturesDefinition, rhi::EMemoryUsage::GPUOnly);

		rhi::TextureDefinition accumulatedSamplesNumTextureDef;
		accumulatedSamplesNumTextureDef.resolution = resolution;
		accumulatedSamplesNumTextureDef.usage      = lib::Flags(rhi::ETextureUsage::SampledTexture, rhi::ETextureUsage::StorageTexture);
		accumulatedSamplesNumTextureDef.format     = rhi::EFragmentFormat::R8_U_Int;
		m_accumulatedSamplesNumTexture        = rdr::ResourcesManager::CreateTextureView(RENDERER_RESOURCE_NAME("RT Reflections Accumulated Samples Num"), accumulatedSamplesNumTextureDef, rhi::EMemoryUsage::GPUOnly);
		m_historyAccumulatedSamplesNumTexture = rdr::ResourcesManager::CreateTextureView(RENDERER_RESOURCE_NAME("RT Reflections Accumulated Samples Num"), accumulatedSamplesNumTextureDef, rhi::EMemoryUsage::GPUOnly);

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

	const rg::RGTextureViewHandle specularTexture = params.specularTexture;
	const rg::RGTextureViewHandle diffuseTexture  = params.diffuseTexture;

	SPT_CHECK(specularTexture.IsValid());
	SPT_CHECK(diffuseTexture.IsValid());

	const math::Vector2u resolution = specularTexture->GetResolution2D();

	const rg::RGTextureViewHandle historySpecularTexture                 = graphBuilder.AcquireExternalTextureView(lib::Ref(m_historySpecularTexture));
	const rg::RGTextureViewHandle historyDiffuseTexture                  = graphBuilder.AcquireExternalTextureView(lib::Ref(m_historyDiffuseTexture));
	const rg::RGTextureViewHandle accumulatedSamplesNumTexture           = graphBuilder.AcquireExternalTextureView(lib::Ref(m_accumulatedSamplesNumTexture));
	const rg::RGTextureViewHandle historyAccumulatedSamplesNumTexture    = graphBuilder.AcquireExternalTextureView(lib::Ref(m_historyAccumulatedSamplesNumTexture));
	const rg::RGTextureViewHandle fastHistorySpecularTexture             = graphBuilder.AcquireExternalTextureView(lib::Ref(m_fastHistorySpecularTexture));
	const rg::RGTextureViewHandle fastHistorySpecularOutputTexture       = graphBuilder.AcquireExternalTextureView(lib::Ref(m_fastHistorySpecularOutputTexture));
	const rg::RGTextureViewHandle fastHistoryDiffuseTexture              = graphBuilder.AcquireExternalTextureView(lib::Ref(m_fastHistoryDiffuseTexture));
	const rg::RGTextureViewHandle fastHistoryDiffuseOutputTexture        = graphBuilder.AcquireExternalTextureView(lib::Ref(m_fastHistoryDiffuseOutputTexture));
	const rg::RGTextureViewHandle temporalVarianceSpecularTexture        = graphBuilder.AcquireExternalTextureView(lib::Ref(m_temporalVarianceSpecularTexture));
	const rg::RGTextureViewHandle historyTemporalVarianceSpecularTexture = graphBuilder.AcquireExternalTextureView(lib::Ref(m_historyTemporalVarianceSpecularTexture));
	const rg::RGTextureViewHandle temporalVarianceDiffuseTexture         = graphBuilder.AcquireExternalTextureView(lib::Ref(m_temporalVarianceDiffuseTexture));
	const rg::RGTextureViewHandle historyTemporalVarianceDiffuseTexture  = graphBuilder.AcquireExternalTextureView(lib::Ref(m_historyTemporalVarianceDiffuseTexture));

	rg::RGTextureViewHandle reprojectionConfidence = CreateReprojectionConfidenceTexture(graphBuilder, resolution);

	if (m_hasValidHistory)
	{
		SPT_CHECK(params.historyNormalsTexture.IsValid());
		SPT_CHECK(params.historyDepthTexture.IsValid());
		SPT_CHECK(params.historyRoughnessTexture.IsValid());

		TemporalAccumulationParameters temporalAccumulationParameters(params.renderView);
		temporalAccumulationParameters.name                                   = m_debugName;
		temporalAccumulationParameters.historyDepthTexture                    = params.historyDepthTexture;
		temporalAccumulationParameters.currentDepthTexture                    = params.currentDepthTexture;
		temporalAccumulationParameters.currentRoughnessTexture                = params.roughnessTexture;
		temporalAccumulationParameters.motionTexture                          = params.motionTexture;
		temporalAccumulationParameters.normalsTexture                         = params.normalsTexture;
		temporalAccumulationParameters.historyNormalsTexture                  = params.historyNormalsTexture;
		temporalAccumulationParameters.currentSpecularTexture                 = specularTexture;
		temporalAccumulationParameters.currentDiffuseTexture                  = diffuseTexture;
		temporalAccumulationParameters.historySpecularTexture                 = historySpecularTexture;
		temporalAccumulationParameters.historyDiffuseTexture                  = historyDiffuseTexture;
		temporalAccumulationParameters.accumulatedSamplesNumTexture           = accumulatedSamplesNumTexture;
		temporalAccumulationParameters.historyAccumulatedSamplesNumTexture    = historyAccumulatedSamplesNumTexture;
		temporalAccumulationParameters.temporalVarianceSpecularTexture        = temporalVarianceSpecularTexture;
		temporalAccumulationParameters.historyTemporalVarianceSpecularTexture = historyTemporalVarianceSpecularTexture;
		temporalAccumulationParameters.temporalVarianceDiffuseTexture         = temporalVarianceDiffuseTexture;
		temporalAccumulationParameters.historyTemporalVarianceDiffuseTexture  = historyTemporalVarianceDiffuseTexture;
		temporalAccumulationParameters.historyRoughnessTexture                = params.historyRoughnessTexture;
		temporalAccumulationParameters.reprojectionConfidenceTexture          = reprojectionConfidence;
		temporalAccumulationParameters.fastHistorySpecularTexture             = fastHistorySpecularTexture;
		temporalAccumulationParameters.fastHistorySpecularOutputTexture       = fastHistorySpecularOutputTexture;
		temporalAccumulationParameters.fastHistoryDiffuseTexture              = fastHistoryDiffuseTexture;
		temporalAccumulationParameters.fastHistoryDiffuseOutputTexture        = fastHistoryDiffuseOutputTexture;

		ApplyTemporalAccumulation(graphBuilder, temporalAccumulationParameters);
	}
	else
	{
		graphBuilder.CopyFullTexture(RG_DEBUG_NAME_FORMATTED("{}: Copy Specular Fast History", m_debugName.AsString()), specularTexture, fastHistorySpecularOutputTexture);
		graphBuilder.CopyFullTexture(RG_DEBUG_NAME_FORMATTED("{}: Copy Diffuse Fast History", m_debugName.AsString()), diffuseTexture, fastHistoryDiffuseOutputTexture);
		graphBuilder.ClearTexture(RG_DEBUG_NAME_FORMATTED("{}: Clear reprojection confidence", m_debugName.AsString()), reprojectionConfidence, rhi::ClearColor(0.f, 0.f, 0.f, 0.f));
		graphBuilder.ClearTexture(RG_DEBUG_NAME_FORMATTED("{}: Clear Specular Moments", m_debugName.AsString()), temporalVarianceSpecularTexture, rhi::ClearColor(0.f, 0.f, 0.f, 0.f));
		graphBuilder.ClearTexture(RG_DEBUG_NAME_FORMATTED("{}: Clear Diffuse Moments", m_debugName.AsString()), temporalVarianceDiffuseTexture, rhi::ClearColor(0.f, 0.f, 0.f, 0.f));
	}

	const rg::RGTextureViewHandle outSpecularTexture = graphBuilder.CreateTextureView(RG_DEBUG_NAME_FORMATTED("{}: Output Specular Texture", m_debugName.AsString()), rg::TextureDef(resolution, specularTexture->GetFormat()));
	const rg::RGTextureViewHandle outDiffuseTexture  = graphBuilder.CreateTextureView(RG_DEBUG_NAME_FORMATTED("{}: Output Diffuse Texture", m_debugName.AsString()), rg::TextureDef(resolution, diffuseTexture->GetFormat()));

	DisocclusionFixParams disocclusionFixParams(params.renderView);
	disocclusionFixParams.debugName                    = m_debugName;
	disocclusionFixParams.accumulatedSamplesNumTexture = accumulatedSamplesNumTexture;
	disocclusionFixParams.normalsTexture               = params.normalsTexture;
	disocclusionFixParams.depthTexture                 = params.currentDepthTexture;
	disocclusionFixParams.roughnessTexture             = params.roughnessTexture;
	disocclusionFixParams.diffuseTexture               = diffuseTexture;
	disocclusionFixParams.outputDiffuseTexture         = historyDiffuseTexture;
	disocclusionFixParams.specularTexture              = specularTexture;
	disocclusionFixParams.outputSpecularTexture        = historySpecularTexture;
	DisocclusionFix(graphBuilder, disocclusionFixParams);

	ClampHistoryParams clampHistoryParams(params.renderView);
	clampHistoryParams.debugName                      = m_debugName;
	clampHistoryParams.accumulatedSamplesNumTexture   = accumulatedSamplesNumTexture;
	clampHistoryParams.depthTexture                   = params.currentDepthTexture;
	clampHistoryParams.normalsTexture                 = params.normalsTexture;
	clampHistoryParams.roughnessTexture               = params.roughnessTexture;
	clampHistoryParams.reprojectionConfidenceTexture  = reprojectionConfidence;
	clampHistoryParams.specularTexture                = specularTexture;
	clampHistoryParams.fastHistorySpecularTexture     = fastHistorySpecularTexture;
	clampHistoryParams.diffuseTexture                 = diffuseTexture;
	clampHistoryParams.fastHistoryDiffuseTexture      =	fastHistoryDiffuseTexture;
	ClampHistory(graphBuilder, clampHistoryParams);

	FireflySuppressionParams fireflySuppressionParams;
	fireflySuppressionParams.debugName                = m_debugName;
	fireflySuppressionParams.inSpecularHitDistTexture = historySpecularTexture;
	fireflySuppressionParams.outSpecularHitDistTexture= specularTexture;
	fireflySuppressionParams.inDiffuseHitDistTexture  = historyDiffuseTexture;
	fireflySuppressionParams.outDiffuseHitDistTexture =	diffuseTexture;
	SuppressFireflies(graphBuilder, fireflySuppressionParams);

	const rg::RGTextureViewHandle temporalSpecularVariance     = CreateVarianceTexture(graphBuilder, RG_DEBUG_NAME("Temporal Specular Variance"), resolution);
	const rg::RGTextureViewHandle intermediateSpecularVariance = CreateVarianceTexture(graphBuilder, RG_DEBUG_NAME("Intermediate Specular Variance"), resolution);

	const rg::RGTextureViewHandle temporalDiffuseVariance     = CreateVarianceTexture(graphBuilder, RG_DEBUG_NAME("Temporal Diffuse Variance"), resolution);
	const rg::RGTextureViewHandle intermediateDiffuseVariance = CreateVarianceTexture(graphBuilder, RG_DEBUG_NAME("Intermediate Diffuse Variance"), resolution);

	TemporalVarianceParams temporalVarianceParams(params.renderView);
	temporalVarianceParams.debugName                    = m_debugName;
	temporalVarianceParams.accumulatedSamplesNumTexture = accumulatedSamplesNumTexture;
	temporalVarianceParams.normalsTexture               = params.normalsTexture;
	temporalVarianceParams.depthTexture                 = params.currentDepthTexture;
	temporalVarianceParams.specularMomentsTexture       = temporalVarianceSpecularTexture;
	temporalVarianceParams.specularTexture              = specularTexture;
	temporalVarianceParams.outSpecularVarianceTexture   = temporalSpecularVariance;
	temporalVarianceParams.diffuseMomentsTexture        = temporalVarianceDiffuseTexture;
	temporalVarianceParams.diffuseTexture               = diffuseTexture;
	temporalVarianceParams.outDiffuseVarianceTexture    = temporalDiffuseVariance;
	ComputeTemporalVariance(graphBuilder, temporalVarianceParams);

	const rg::RGTextureViewHandle varianceEstimation = CreateVarianceEstimationTexture(graphBuilder, RG_DEBUG_NAME("Variance Estimation"), resolution);

	VarianceEstimationParams varianceEstimationParams(params.renderView);
	varianceEstimationParams.debugName                    = m_debugName;
	varianceEstimationParams.accumulatedSamplesNumTexture = accumulatedSamplesNumTexture;
	varianceEstimationParams.normalsTexture               = params.normalsTexture;
	varianceEstimationParams.depthTexture                 = params.currentDepthTexture;
	varianceEstimationParams.roughnessTexture             = params.roughnessTexture;
	varianceEstimationParams.specularVarianceTexture      = temporalVarianceSpecularTexture;
	varianceEstimationParams.diffuseVarianceTexture       = temporalVarianceDiffuseTexture;
	varianceEstimationParams.outEstimatedVarianceTexture  = varianceEstimation;

	EstimateVariance(graphBuilder, varianceEstimationParams);

	SpatialFilterParams spatialParams;
	spatialParams.inSpecular                   = specularTexture;
	spatialParams.outSpecular                  = outSpecularTexture;
	spatialParams.inSpecularVariance           = temporalSpecularVariance;
	spatialParams.inDiffuse                    = diffuseTexture;
	spatialParams.outDiffuse                   = outDiffuseTexture;
	spatialParams.inDiffuseVariance            = temporalDiffuseVariance;
	spatialParams.intermediateSpecularVariance = intermediateSpecularVariance;
	spatialParams.intermediateDiffuseVariance  = intermediateDiffuseVariance;
	spatialParams.historySamplesNum            = historyAccumulatedSamplesNumTexture;
	spatialParams.reprojectionConfidence       = reprojectionConfidence;

	ApplySpatialFilter(graphBuilder, spatialParams, params);

	Result result;
	result.denoisedSpecular   = outSpecularTexture;
	result.denoisedDiffuse    = outDiffuseTexture;
	result.varianceEstimation = varianceEstimation;

	std::swap(m_accumulatedSamplesNumTexture, m_historyAccumulatedSamplesNumTexture);

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

	SRATrousFilterParams aTrousParams(params.renderView);
	aTrousParams.name                          = m_debugName;
	aTrousParams.linearDepthTexture            = params.linearDepthTexture;
	aTrousParams.depthTexture                  = params.currentDepthTexture;
	aTrousParams.normalsTexture                = params.normalsTexture;
	aTrousParams.roughnessTexture              = params.roughnessTexture;
	aTrousParams.reprojectionConfidenceTexture = spatialParams.reprojectionConfidence;
	aTrousParams.historyFramesNumTexture       = spatialParams.historySamplesNum;
	aTrousParams.enableDetailPreservation      = params.detailPreservingSpatialFilter;

	SRATrousPass pass;
	pass.inSpecularLuminance  = spatialParams.inSpecular;
	pass.inDiffuseLuminance   = spatialParams.inDiffuse;
	pass.inSpecularVariance   = spatialParams.inSpecularVariance;
	pass.inDiffuseVariance    = spatialParams.inDiffuseVariance;
	pass.outSpecularLuminance = spatialParams.outSpecular;
	pass.outDiffuseLuminance  = spatialParams.outDiffuse;
	pass.outSpecularVariance  = spatialParams.intermediateSpecularVariance;
	pass.outDiffuseVariance   = spatialParams.intermediateDiffuseVariance;

	ApplyATrousFilter(graphBuilder, aTrousParams,   pass);
	ApplyATrousFilter(graphBuilder, aTrousParams, ++pass);
	ApplyATrousFilter(graphBuilder, aTrousParams, ++pass);
	ApplyATrousFilter(graphBuilder, aTrousParams, ++pass);
	ApplyATrousFilter(graphBuilder, aTrousParams, ++pass);
}

} // spt::rsc::sr_denoiser
