#include "SRDenoiser.h"
#include "Types/Texture.h"
#include "RenderGraphBuilder.h"
#include "ResourcesManager.h"
#include "SRTemporalAccumulation.h"
#include "SRComputeStdDev.h"
#include "SRATrousFilter.h"


namespace spt::rsc::sr_denoiser
{

Denoiser::Denoiser(rg::RenderGraphDebugName debugName)
	: m_debugName(debugName)
{ }

void Denoiser::Denoise(rg::RenderGraphBuilder& graphBuilder, rg::RGTextureViewHandle denoisedTexture, const Params& params)
{
	SPT_PROFILER_FUNCTION();

	UpdateResources(graphBuilder, denoisedTexture, params);

	DenoiseImpl(graphBuilder, denoisedTexture, params);
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

		m_hasValidHistory = false;
	}
}

void Denoiser::DenoiseImpl(rg::RenderGraphBuilder& graphBuilder, rg::RGTextureViewHandle denoisedTexture, const Params& params)
{
	SPT_PROFILER_FUNCTION();

	const rg::RGTextureViewHandle historyTexture                      = graphBuilder.AcquireExternalTextureView(lib::Ref(m_historyTexture));
	const rg::RGTextureViewHandle accumulatedSamplesNumTexture        = graphBuilder.AcquireExternalTextureView(lib::Ref(m_accumulatedSamplesNumTexture));
	const rg::RGTextureViewHandle historyAccumulatedSamplesNumTexture = graphBuilder.AcquireExternalTextureView(lib::Ref(m_historyAccumulatedSamplesNumTexture));
	const rg::RGTextureViewHandle temporalVarianceTexture             = graphBuilder.AcquireExternalTextureView(lib::Ref(m_temporalVarianceTexture));
	const rg::RGTextureViewHandle historyTemporalVarianceTexture      = graphBuilder.AcquireExternalTextureView(lib::Ref(m_historyTemporalVarianceTexture));

	if (m_hasValidHistory)
	{
		TemporalAccumulationParameters temporalAccumulationParameters(params.renderView);
		temporalAccumulationParameters.name                                = m_debugName;
		temporalAccumulationParameters.historyDepthTexture                 = params.historyDepthTexture;
		temporalAccumulationParameters.currentDepthTexture                 = params.currentDepthTexture;
		temporalAccumulationParameters.specularColorAndRoughnessTexture    = params.specularColorAndRoughnessTexture;
		temporalAccumulationParameters.motionTexture                       = params.motionTexture;
		temporalAccumulationParameters.normalsTexture                      = params.normalsTexture;
		temporalAccumulationParameters.currentTexture                      = denoisedTexture;
		temporalAccumulationParameters.historyTexture                      = historyTexture;
		temporalAccumulationParameters.accumulatedSamplesNumTexture        = accumulatedSamplesNumTexture;
		temporalAccumulationParameters.historyAccumulatedSamplesNumTexture = historyAccumulatedSamplesNumTexture;
		temporalAccumulationParameters.temporalVarianceTexture             = temporalVarianceTexture;
		temporalAccumulationParameters.historyTemporalVarianceTexture      = historyTemporalVarianceTexture;
		temporalAccumulationParameters.ddgiSceneDS                         = params.ddgiSceneDS;

		ApplyTemporalAccumulation(graphBuilder, temporalAccumulationParameters);
	}

	StdDevParams stdDevParams(params.renderView);
	stdDevParams.debugName                    = m_debugName;
	stdDevParams.momentsTexture               = temporalVarianceTexture;
	stdDevParams.accumulatedSamplesNumTexture = accumulatedSamplesNumTexture;
	stdDevParams.normalsTexture               = params.normalsTexture;
	stdDevParams.depthTexture                 = params.currentDepthTexture;
	stdDevParams.luminanceTexture             = denoisedTexture;
	const rg::RGTextureViewHandle stdDevTexture = ComputeStandardDeviation(graphBuilder, stdDevParams);

	ApplySpatialFilter(graphBuilder, denoisedTexture, historyTexture, stdDevTexture, params);

	std::swap(m_accumulatedSamplesNumTexture, m_historyAccumulatedSamplesNumTexture);
	std::swap(m_temporalVarianceTexture, m_historyTemporalVarianceTexture);
	m_hasValidHistory = true;
}

void Denoiser::ApplySpatialFilter(rg::RenderGraphBuilder& graphBuilder, rg::RGTextureViewHandle denoisedTexture, rg::RGTextureViewHandle historyTexture, rg::RGTextureViewHandle stdDevTexture, const Params& params)
{
	SPT_PROFILER_FUNCTION();

	const rg::RGTextureViewHandle tempTexture = graphBuilder.CreateTextureView(RG_DEBUG_NAME_FORMATTED("{} SR Temp Spatial Filter Texture", m_debugName.AsString()),
																			   rg::TextureDef(denoisedTexture->GetResolution2D(), denoisedTexture->GetFormat()));
	const rg::RGTextureViewHandle tempTexture2 = graphBuilder.CreateTextureView(RG_DEBUG_NAME_FORMATTED("{} SR Temp Spatial Filter Texture2", m_debugName.AsString()),
																			   rg::TextureDef(denoisedTexture->GetResolution2D(), denoisedTexture->GetFormat()));

	SRATrousFilterParams aTrousParams(params.renderView);
	aTrousParams.name           = m_debugName;
	aTrousParams.depthTexture   = params.currentDepthTexture;
	aTrousParams.normalsTexture = params.normalsTexture;
	aTrousParams.stdDevTexture  = stdDevTexture;
	aTrousParams.specularColorRoughnessTexture = params.specularColorAndRoughnessTexture;

	Uint32 iterationIdx = 0u;
	ApplyATrousFilter(graphBuilder, aTrousParams, denoisedTexture, historyTexture, iterationIdx++);
	ApplyATrousFilter(graphBuilder, aTrousParams, historyTexture, denoisedTexture, iterationIdx++);
	ApplyATrousFilter(graphBuilder, aTrousParams, denoisedTexture, tempTexture, iterationIdx++);
	ApplyATrousFilter(graphBuilder, aTrousParams, tempTexture, tempTexture2, iterationIdx++);
	ApplyATrousFilter(graphBuilder, aTrousParams, tempTexture2, denoisedTexture, iterationIdx++);
}

} // spt::rsc::sr_denoiser
