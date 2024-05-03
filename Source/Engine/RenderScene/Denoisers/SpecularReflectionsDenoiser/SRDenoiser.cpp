#include "SRDenoiser.h"
#include "Types/Texture.h"
#include "RenderGraphBuilder.h"
#include "ResourcesManager.h"
#include "SRTemporalAccumulation.h"
#include "SRComputeStdDev.h"
#include "SRATrousFilter.h"
#include "Denoisers/SpecularReflectionsDenoiser/SRDisocclussionFix.h"
#include "Denoisers/SpecularReflectionsDenoiser/SRClampHistory.h"
#include "Denoisers/SpecularReflectionsDenoiser/SRFireflySuppression.h"


namespace spt::rsc::sr_denoiser
{

Denoiser::Denoiser(rg::RenderGraphDebugName debugName)
	: m_debugName(debugName)
{ }

rg::RGTextureViewHandle Denoiser::Denoise(rg::RenderGraphBuilder& graphBuilder, rg::RGTextureViewHandle denoisedTexture, const Params& params)
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
		historyTextureDefinition.usage      = lib::Flags(rhi::ETextureUsage::SampledTexture, rhi::ETextureUsage::StorageTexture);
#if SPT_DEBUG || SPT_DEVELOPMENT
		lib::AddFlag(historyTextureDefinition.usage, rhi::ETextureUsage::TransferSource);
#endif
		historyTextureDefinition.format     = denoisedTexture->GetFormat();
		m_historyTexture = rdr::ResourcesManager::CreateTextureView(RENDERER_RESOURCE_NAME("SR Reflections History"), historyTextureDefinition, rhi::EMemoryUsage::GPUOnly);

		m_fastHistoryTexture       = rdr::ResourcesManager::CreateTextureView(RENDERER_RESOURCE_NAME("SR Reflections Fast History"), historyTextureDefinition, rhi::EMemoryUsage::GPUOnly);
		m_fastHistoryOutputTexture = rdr::ResourcesManager::CreateTextureView(RENDERER_RESOURCE_NAME("SR Reflections Fast History"), historyTextureDefinition, rhi::EMemoryUsage::GPUOnly);

		rhi::TextureDefinition accumulatedSamplesNumTextureDef;
		accumulatedSamplesNumTextureDef.resolution = resolution;
		accumulatedSamplesNumTextureDef.usage      = lib::Flags(rhi::ETextureUsage::SampledTexture, rhi::ETextureUsage::StorageTexture);
#if SPT_DEBUG || SPT_DEVELOPMENT
		lib::AddFlag(accumulatedSamplesNumTextureDef.usage, rhi::ETextureUsage::TransferSource);
#endif
		accumulatedSamplesNumTextureDef.format     = rhi::EFragmentFormat::R8_U_Int;
		m_accumulatedSamplesNumTexture = rdr::ResourcesManager::CreateTextureView(RENDERER_RESOURCE_NAME("SR Reflections Accumulated Samples Num"),
																				  accumulatedSamplesNumTextureDef, rhi::EMemoryUsage::GPUOnly);
		
		m_historyAccumulatedSamplesNumTexture = rdr::ResourcesManager::CreateTextureView(RENDERER_RESOURCE_NAME("SR Reflections Accumulated Samples Num"),
																				  accumulatedSamplesNumTextureDef, rhi::EMemoryUsage::GPUOnly);

		rhi::TextureDefinition temporalVarianceTextureDef;
		temporalVarianceTextureDef.resolution = resolution;
		temporalVarianceTextureDef.usage      = lib::Flags(rhi::ETextureUsage::SampledTexture, rhi::ETextureUsage::StorageTexture);
#if SPT_DEBUG || SPT_DEVELOPMENT
		lib::AddFlag(temporalVarianceTextureDef.usage, rhi::ETextureUsage::TransferSource);
#endif
		temporalVarianceTextureDef.format     = rhi::EFragmentFormat::RG16_S_Float;
		m_temporalVarianceTexture = rdr::ResourcesManager::CreateTextureView(RENDERER_RESOURCE_NAME("SR Reflections Temporal Variance"),
																			 temporalVarianceTextureDef, rhi::EMemoryUsage::GPUOnly);
		
		m_historyTemporalVarianceTexture = rdr::ResourcesManager::CreateTextureView(RENDERER_RESOURCE_NAME("SR Reflections Temporal Variance"),
																					temporalVarianceTextureDef, rhi::EMemoryUsage::GPUOnly);



		rhi::TextureDefinition roughnessTextureDef;
		roughnessTextureDef.resolution = resolution;
		roughnessTextureDef.usage      = lib::Flags(rhi::ETextureUsage::SampledTexture, rhi::ETextureUsage::StorageTexture);
#if SPT_DEBUG || SPT_DEVELOPMENT
		lib::AddFlag(roughnessTextureDef.usage, rhi::ETextureUsage::TransferSource);
#endif
		roughnessTextureDef.format     = rhi::EFragmentFormat::R8_UN_Float;
		m_roughnessTexture = rdr::ResourcesManager::CreateTextureView(RENDERER_RESOURCE_NAME("SR Reflections Roughness"),
																	  roughnessTextureDef, rhi::EMemoryUsage::GPUOnly);
		
		m_historyRoughnessTexture = rdr::ResourcesManager::CreateTextureView(RENDERER_RESOURCE_NAME("SR Reflections Roughness"),
																			 roughnessTextureDef, rhi::EMemoryUsage::GPUOnly);

		m_hasValidHistory = false;
	}
}

rg::RGTextureViewHandle Denoiser::DenoiseImpl(rg::RenderGraphBuilder& graphBuilder, rg::RGTextureViewHandle denoisedTexture, const Params& params)
{
	SPT_PROFILER_FUNCTION();

	const math::Vector2u resolution = denoisedTexture->GetResolution2D();

	const rg::RGTextureViewHandle historyTexture                      = graphBuilder.AcquireExternalTextureView(lib::Ref(m_historyTexture));
	const rg::RGTextureViewHandle accumulatedSamplesNumTexture        = graphBuilder.AcquireExternalTextureView(lib::Ref(m_accumulatedSamplesNumTexture));
	const rg::RGTextureViewHandle historyAccumulatedSamplesNumTexture = graphBuilder.AcquireExternalTextureView(lib::Ref(m_historyAccumulatedSamplesNumTexture));
	const rg::RGTextureViewHandle temporalVarianceTexture             = graphBuilder.AcquireExternalTextureView(lib::Ref(m_temporalVarianceTexture));
	const rg::RGTextureViewHandle historyTemporalVarianceTexture      = graphBuilder.AcquireExternalTextureView(lib::Ref(m_historyTemporalVarianceTexture));
	const rg::RGTextureViewHandle outputRoughnessTexture              = graphBuilder.AcquireExternalTextureView(lib::Ref(m_roughnessTexture));
	const rg::RGTextureViewHandle historyRoughnessTexture             = graphBuilder.AcquireExternalTextureView(lib::Ref(m_historyRoughnessTexture));
	const rg::RGTextureViewHandle fastHistoryTexture                  = graphBuilder.AcquireExternalTextureView(lib::Ref(m_fastHistoryTexture));
	const rg::RGTextureViewHandle fastHistoryOutputTexture            = graphBuilder.AcquireExternalTextureView(lib::Ref(m_fastHistoryOutputTexture));

	rg::RGTextureViewHandle reprojectionConfidence = CreateReprojectionConfidenceTexture(graphBuilder, resolution);

	if (m_hasValidHistory)
	{
		SPT_CHECK(params.historyNormalsTexture.IsValid());

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
		temporalAccumulationParameters.historyRoughnessTexture             = historyRoughnessTexture;
		temporalAccumulationParameters.outputRoughnessTexture              = outputRoughnessTexture;
		temporalAccumulationParameters.reprojectionConfidenceTexture       = reprojectionConfidence;
		temporalAccumulationParameters.fastHistoryTexture                  = fastHistoryTexture;
		temporalAccumulationParameters.fastHistoryOutputTexture            = fastHistoryOutputTexture;

		ApplyTemporalAccumulation(graphBuilder, temporalAccumulationParameters);
	}
	else
	{
		graphBuilder.CopyFullTexture(RG_DEBUG_NAME_FORMATTED("{}: Copy Roughness", m_debugName.AsString()), params.roughnessTexture, outputRoughnessTexture);
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
	disocclusionFixParams.outputLuminanceTexture       = outputTexture;
	DisocclusionFix(graphBuilder, disocclusionFixParams);

	ClampHistoryParams clampHistoryParams(params.renderView);
	clampHistoryParams.debugName                      = m_debugName;
	clampHistoryParams.luminanceAndHitDistanceTexture = outputTexture;
	clampHistoryParams.accumulatedSamplesNumTexture   = accumulatedSamplesNumTexture;
	clampHistoryParams.fastHistoryLuminanceTexture    = fastHistoryOutputTexture;
	clampHistoryParams.depthTexture                   = params.currentDepthTexture;
	clampHistoryParams.normalsTexture                 = params.normalsTexture;
	clampHistoryParams.roughnessTexture               = params.roughnessTexture;
	clampHistoryParams.reprojectionConfidenceTexture  = reprojectionConfidence;
	ClampHistory(graphBuilder, clampHistoryParams);

	graphBuilder.CopyFullTexture(RG_DEBUG_NAME_FORMATTED("{}: Copy History", m_debugName.AsString()), outputTexture, historyTexture);

	FireflySuppressionParams fireflySuppressionParams;
	fireflySuppressionParams.debugName                    = m_debugName;
	fireflySuppressionParams.inputLuminanceHitDisTexture  = outputTexture;
	fireflySuppressionParams.outputLuminanceHitDisTexture = denoisedTexture;
	SuppressFireflies(graphBuilder, fireflySuppressionParams);

	StdDevParams stdDevParams(params.renderView);
	stdDevParams.debugName                    = m_debugName;
	stdDevParams.momentsTexture               = temporalVarianceTexture;
	stdDevParams.accumulatedSamplesNumTexture = accumulatedSamplesNumTexture;
	stdDevParams.normalsTexture               = params.normalsTexture;
	stdDevParams.depthTexture                 = params.currentDepthTexture;
	stdDevParams.luminanceTexture             = denoisedTexture;
	const rg::RGTextureViewHandle stdDevTexture = ComputeStandardDeviation(graphBuilder, stdDevParams);

	ApplySpatialFilter(graphBuilder,
					   denoisedTexture,
					   outputTexture,
					   stdDevTexture,
					   historyAccumulatedSamplesNumTexture,
					   reprojectionConfidence,
					   params);

	std::swap(m_accumulatedSamplesNumTexture, m_historyAccumulatedSamplesNumTexture);
	std::swap(m_temporalVarianceTexture, m_historyTemporalVarianceTexture);
	std::swap(m_roughnessTexture, m_historyRoughnessTexture);
	std::swap(m_fastHistoryTexture, m_fastHistoryOutputTexture);
	m_hasValidHistory = true;

	return outputTexture;
}

void Denoiser::ApplySpatialFilter(rg::RenderGraphBuilder& graphBuilder, rg::RGTextureViewHandle input, rg::RGTextureViewHandle output, rg::RGTextureViewHandle stdDev, rg::RGTextureViewHandle historySamplesNum, rg::RGTextureViewHandle reprojectionConfidence, const Params& params)
{
	SPT_PROFILER_FUNCTION();

	SRATrousFilterParams aTrousParams(params.renderView);
	aTrousParams.name                          = m_debugName;
	aTrousParams.depthTexture                  = params.currentDepthTexture;
	aTrousParams.normalsTexture                = params.normalsTexture;
	aTrousParams.stdDevTexture                 = stdDev;
	aTrousParams.roughnessTexture              = params.roughnessTexture;
	aTrousParams.reprojectionConfidenceTexture = reprojectionConfidence;
	aTrousParams.historyFramesNumTexture       = historySamplesNum;

	Uint32 iterationIdx = 0u;
	ApplyATrousFilter(graphBuilder, aTrousParams, input, output, iterationIdx++);
	ApplyATrousFilter(graphBuilder, aTrousParams, output, input, iterationIdx++);
	ApplyATrousFilter(graphBuilder, aTrousParams, input, output, iterationIdx++);
	ApplyATrousFilter(graphBuilder, aTrousParams, output, input, iterationIdx++);
	ApplyATrousFilter(graphBuilder, aTrousParams, input, output, iterationIdx++);
}

} // spt::rsc::sr_denoiser
