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

Denoiser::Result Denoiser::Denoise(rg::RenderGraphBuilder& graphBuilder, rg::RGTextureViewHandle denoisedTexture, const Params& params)
{
	SPT_PROFILER_FUNCTION();

	UpdateResources(graphBuilder, denoisedTexture, params);

	return DenoiseImpl(graphBuilder, denoisedTexture, params);
}

void Denoiser::UpdateResources(rg::RenderGraphBuilder& graphBuilder, rg::RGTextureViewHandle denoisedTexture, const Params& params)
{
	SPT_PROFILER_FUNCTION();

	const math::Vector2u resolution = denoisedTexture->GetResolution2D();

	if (!m_historyTexture || m_historyTexture->GetResolution2D() != resolution)
	{
		rhi::TextureDefinition historyTextureDefinition;
		historyTextureDefinition.resolution = resolution;
		historyTextureDefinition.usage      = lib::Flags(rhi::ETextureUsage::SampledTexture, rhi::ETextureUsage::StorageTexture, rhi::ETextureUsage::TransferDest);
		historyTextureDefinition.format     = denoisedTexture->GetFormat();
		m_historyTexture = rdr::ResourcesManager::CreateTextureView(RENDERER_RESOURCE_NAME("SR Reflections History"), historyTextureDefinition, rhi::EMemoryUsage::GPUOnly);

		m_fastHistoryTexture       = rdr::ResourcesManager::CreateTextureView(RENDERER_RESOURCE_NAME("SR Reflections Fast History"), historyTextureDefinition, rhi::EMemoryUsage::GPUOnly);
		m_fastHistoryOutputTexture = rdr::ResourcesManager::CreateTextureView(RENDERER_RESOURCE_NAME("SR Reflections Fast History"), historyTextureDefinition, rhi::EMemoryUsage::GPUOnly);

		rhi::TextureDefinition accumulatedSamplesNumTextureDef;
		accumulatedSamplesNumTextureDef.resolution = resolution;
		accumulatedSamplesNumTextureDef.usage      = lib::Flags(rhi::ETextureUsage::SampledTexture, rhi::ETextureUsage::StorageTexture);
		accumulatedSamplesNumTextureDef.format     = rhi::EFragmentFormat::R8_U_Int;
		m_accumulatedSamplesNumTexture = rdr::ResourcesManager::CreateTextureView(RENDERER_RESOURCE_NAME("SR Reflections Accumulated Samples Num"),
																				  accumulatedSamplesNumTextureDef, rhi::EMemoryUsage::GPUOnly);
		
		m_historyAccumulatedSamplesNumTexture = rdr::ResourcesManager::CreateTextureView(RENDERER_RESOURCE_NAME("SR Reflections Accumulated Samples Num"),
																				  accumulatedSamplesNumTextureDef, rhi::EMemoryUsage::GPUOnly);

		rhi::TextureDefinition temporalVarianceTextureDef;
		temporalVarianceTextureDef.resolution = resolution;
		temporalVarianceTextureDef.usage      = lib::Flags(rhi::ETextureUsage::SampledTexture, rhi::ETextureUsage::StorageTexture, rhi::ETextureUsage::TransferDest);
		temporalVarianceTextureDef.format     = rhi::EFragmentFormat::RG16_S_Float;
		m_temporalVarianceTexture = rdr::ResourcesManager::CreateTextureView(RENDERER_RESOURCE_NAME("SR Reflections Temporal Variance"),
																			 temporalVarianceTextureDef, rhi::EMemoryUsage::GPUOnly);
		
		m_historyTemporalVarianceTexture = rdr::ResourcesManager::CreateTextureView(RENDERER_RESOURCE_NAME("SR Reflections Temporal Variance"),
																					temporalVarianceTextureDef, rhi::EMemoryUsage::GPUOnly);

		m_hasValidHistory = false;
	}
}

Denoiser::Result Denoiser::DenoiseImpl(rg::RenderGraphBuilder& graphBuilder, rg::RGTextureViewHandle denoisedTexture, const Params& params)
{
	SPT_PROFILER_FUNCTION();

	SPT_RG_DIAGNOSTICS_SCOPE(graphBuilder, "RT Denoiser");

	const math::Vector2u resolution = denoisedTexture->GetResolution2D();

	const rg::RGTextureViewHandle historyTexture                      = graphBuilder.AcquireExternalTextureView(lib::Ref(m_historyTexture));
	const rg::RGTextureViewHandle accumulatedSamplesNumTexture        = graphBuilder.AcquireExternalTextureView(lib::Ref(m_accumulatedSamplesNumTexture));
	const rg::RGTextureViewHandle historyAccumulatedSamplesNumTexture = graphBuilder.AcquireExternalTextureView(lib::Ref(m_historyAccumulatedSamplesNumTexture));
	const rg::RGTextureViewHandle temporalVarianceTexture             = graphBuilder.AcquireExternalTextureView(lib::Ref(m_temporalVarianceTexture));
	const rg::RGTextureViewHandle historyTemporalVarianceTexture      = graphBuilder.AcquireExternalTextureView(lib::Ref(m_historyTemporalVarianceTexture));
	const rg::RGTextureViewHandle fastHistoryTexture                  = graphBuilder.AcquireExternalTextureView(lib::Ref(m_fastHistoryTexture));
	const rg::RGTextureViewHandle fastHistoryOutputTexture            = graphBuilder.AcquireExternalTextureView(lib::Ref(m_fastHistoryOutputTexture));

	rg::RGTextureViewHandle reprojectionConfidence = CreateReprojectionConfidenceTexture(graphBuilder, resolution);

	if (m_hasValidHistory)
	{
		SPT_CHECK(params.historyNormalsTexture.IsValid());
		SPT_CHECK(params.historyDepthTexture.IsValid());
		SPT_CHECK(params.historyRoughnessTexture.IsValid());

		TemporalAccumulationParameters temporalAccumulationParameters(params.renderView);
		temporalAccumulationParameters.name                                = m_debugName;
		temporalAccumulationParameters.historyDepthTexture                 = params.historyDepthTexture;
		temporalAccumulationParameters.currentDepthTexture                 = params.currentDepthTexture;
		temporalAccumulationParameters.currentRoughnessTexture             = params.roughnessTexture;
		temporalAccumulationParameters.motionTexture                       = params.motionTexture;
		temporalAccumulationParameters.normalsTexture                      = params.normalsTexture;
		temporalAccumulationParameters.historyNormalsTexture               = params.historyNormalsTexture;
		temporalAccumulationParameters.currentTexture                      = denoisedTexture;
		temporalAccumulationParameters.historyTexture                      = historyTexture;
		temporalAccumulationParameters.accumulatedSamplesNumTexture        = accumulatedSamplesNumTexture;
		temporalAccumulationParameters.historyAccumulatedSamplesNumTexture = historyAccumulatedSamplesNumTexture;
		temporalAccumulationParameters.temporalVarianceTexture             = temporalVarianceTexture;
		temporalAccumulationParameters.historyTemporalVarianceTexture      = historyTemporalVarianceTexture;
		temporalAccumulationParameters.historyRoughnessTexture             = params.historyRoughnessTexture;
		temporalAccumulationParameters.reprojectionConfidenceTexture       = reprojectionConfidence;
		temporalAccumulationParameters.fastHistoryTexture                  = fastHistoryTexture;
		temporalAccumulationParameters.fastHistoryOutputTexture            = fastHistoryOutputTexture;

		ApplyTemporalAccumulation(graphBuilder, temporalAccumulationParameters);
	}
	else
	{
		graphBuilder.CopyFullTexture(RG_DEBUG_NAME_FORMATTED("{}: Copy Fast History", m_debugName.AsString()), denoisedTexture, fastHistoryOutputTexture);
		graphBuilder.ClearTexture(RG_DEBUG_NAME_FORMATTED("{}: Clear reprojection confidence", m_debugName.AsString()), reprojectionConfidence, rhi::ClearColor(0.f, 0.f, 0.f, 0.f));
		graphBuilder.ClearTexture(RG_DEBUG_NAME_FORMATTED("{}: Clear Moments", m_debugName.AsString()), temporalVarianceTexture, rhi::ClearColor(0.f, 0.f, 0.f, 0.f));
	}

	const rg::RGTextureViewHandle outputTexture = graphBuilder.CreateTextureView(RG_DEBUG_NAME_FORMATTED("{}: Output Texture", m_debugName.AsString()), rg::TextureDef(resolution, denoisedTexture->GetFormat()));

	DisocclusionFixParams disocclusionFixParams(params.renderView);
	disocclusionFixParams.debugName                    = m_debugName;
	disocclusionFixParams.accumulatedSamplesNumTexture = accumulatedSamplesNumTexture;
	disocclusionFixParams.normalsTexture               = params.normalsTexture;
	disocclusionFixParams.depthTexture                 = params.currentDepthTexture;
	disocclusionFixParams.roughnessTexture             = params.roughnessTexture;
	disocclusionFixParams.luminanceTexture             = denoisedTexture;
	disocclusionFixParams.outputLuminanceTexture       = historyTexture;
	DisocclusionFix(graphBuilder, disocclusionFixParams);

	ClampHistoryParams clampHistoryParams(params.renderView);
	clampHistoryParams.debugName                      = m_debugName;
	clampHistoryParams.luminanceAndHitDistanceTexture = historyTexture;
	clampHistoryParams.accumulatedSamplesNumTexture   = accumulatedSamplesNumTexture;
	clampHistoryParams.fastHistoryLuminanceTexture    = fastHistoryOutputTexture;
	clampHistoryParams.depthTexture                   = params.currentDepthTexture;
	clampHistoryParams.normalsTexture                 = params.normalsTexture;
	clampHistoryParams.roughnessTexture               = params.roughnessTexture;
	clampHistoryParams.reprojectionConfidenceTexture  = reprojectionConfidence;
	ClampHistory(graphBuilder, clampHistoryParams);

	FireflySuppressionParams fireflySuppressionParams;
	fireflySuppressionParams.debugName                    = m_debugName;
	fireflySuppressionParams.inputLuminanceHitDisTexture  = historyTexture;
	fireflySuppressionParams.outputLuminanceHitDisTexture = denoisedTexture;
	SuppressFireflies(graphBuilder, fireflySuppressionParams);

	rg::RGTextureViewHandle temporalVariance = CreateVarianceTexture(graphBuilder, resolution);
	rg::RGTextureViewHandle tempVariance     = CreateVarianceTexture(graphBuilder, resolution);

	TemporalVarianceParams temporalVarianceParams(params.renderView);
	temporalVarianceParams.debugName                    = m_debugName;
	temporalVarianceParams.momentsTexture               = temporalVarianceTexture;
	temporalVarianceParams.accumulatedSamplesNumTexture = accumulatedSamplesNumTexture;
	temporalVarianceParams.normalsTexture               = params.normalsTexture;
	temporalVarianceParams.depthTexture                 = params.currentDepthTexture;
	temporalVarianceParams.luminanceTexture             = denoisedTexture;
	temporalVarianceParams.outputVarianceTexture        = temporalVariance;
	ComputeTemporalVariance(graphBuilder, temporalVarianceParams);

	const rg::RGTextureViewHandle varianceEstimation = CreateVarianceTexture(graphBuilder, resolution);

	VarianceEstimationParams varianceEstimationParams(params.renderView);
	varianceEstimationParams.debugName = m_debugName;
	varianceEstimationParams.varianceTexture = temporalVariance;
	varianceEstimationParams.accumulatedSamplesNumTexture = accumulatedSamplesNumTexture;
	varianceEstimationParams.normalsTexture = params.normalsTexture;
	varianceEstimationParams.depthTexture = params.currentDepthTexture;
	varianceEstimationParams.roughnessTexture = params.roughnessTexture;
	varianceEstimationParams.tempVarianceTexture = tempVariance;
	varianceEstimationParams.outputEstimatedVarianceTexture = varianceEstimation;
	EstimateVariance(graphBuilder, varianceEstimationParams);

	SpatialFilterParams spatialParams;
	spatialParams.input                  = denoisedTexture;
	spatialParams.output                 = outputTexture;
	spatialParams.inputVariance          = temporalVariance;
	spatialParams.tempVariance           = tempVariance;
	spatialParams.historySamplesNum      = historyAccumulatedSamplesNumTexture;
	spatialParams.reprojectionConfidence = reprojectionConfidence;

	ApplySpatialFilter(graphBuilder, spatialParams, params);

	Result result;
	result.denoiserOutput     = outputTexture;
	result.varianceEstimation = varianceEstimation;

	std::swap(m_accumulatedSamplesNumTexture, m_historyAccumulatedSamplesNumTexture);
	std::swap(m_temporalVarianceTexture, m_historyTemporalVarianceTexture);
	std::swap(m_fastHistoryTexture, m_fastHistoryOutputTexture);
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
	pass.inputLuminance  = spatialParams.input;
	pass.outputLuminance = spatialParams.output;

	ApplyATrousFilter(graphBuilder, aTrousParams,   pass, spatialParams.inputVariance, spatialParams.tempVariance);
	ApplyATrousFilter(graphBuilder, aTrousParams, ++pass, spatialParams.tempVariance, spatialParams.inputVariance);
	ApplyATrousFilter(graphBuilder, aTrousParams, ++pass, spatialParams.inputVariance, spatialParams.tempVariance);
	ApplyATrousFilter(graphBuilder, aTrousParams, ++pass, spatialParams.tempVariance, spatialParams.inputVariance);
	ApplyATrousFilter(graphBuilder, aTrousParams, ++pass, spatialParams.inputVariance, spatialParams.tempVariance);
}

} // spt::rsc::sr_denoiser
