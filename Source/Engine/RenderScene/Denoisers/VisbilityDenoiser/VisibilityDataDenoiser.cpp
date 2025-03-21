#include "VisibilityDataDenoiser.h"
#include "RenderGraphBuilder.h"
#include "ResourcesManager.h"
#include "Filters/VisibilityTemporalFilter.h"
#include "Filters/VisibilitySpatialATrousFilter.h"
#include "Utils/VisibilityMomentsPass.h"


namespace spt::rsc::visibility_denoiser
{

Denoiser::Denoiser(rg::RenderGraphDebugName debugName)
	: m_debugName(debugName)
	, m_hasValidHistory(false)
{ }

void Denoiser::Denoise(rg::RenderGraphBuilder& graphBuilder, rg::RGTextureViewHandle denoisedTexture, const Params& params)
{
	UpdateResources(graphBuilder, denoisedTexture, params);

	DenoiseImpl(graphBuilder, denoisedTexture, params);
}

void Denoiser::UpdateResources(rg::RenderGraphBuilder& graphBuilder, rg::RGTextureViewHandle denoisedTexture, const Params& params)
{
	SPT_PROFILER_FUNCTION();

	if (!m_historyTexture || m_historyTexture->GetResolution2D() != denoisedTexture->GetResolution2D())
	{
		rhi::TextureDefinition historyTextureDef;
		historyTextureDef.resolution	= denoisedTexture->GetResolution2D();
		historyTextureDef.format		= denoisedTexture->GetFormat();
		historyTextureDef.usage			= lib::Flags(rhi::ETextureUsage::StorageTexture, rhi::ETextureUsage::SampledTexture, rhi::ETextureUsage::TransferDest);
		m_historyTexture = rdr::ResourcesManager::CreateTextureView(RENDERER_RESOURCE_NAME_FORMATTED("{}: History Texture", m_debugName.AsString()), historyTextureDef, rhi::EMemoryUsage::GPUOnly);

		rhi::TextureDefinition samplesNumTexturesDef;
		samplesNumTexturesDef.resolution	= denoisedTexture->GetResolution2D();
		samplesNumTexturesDef.format		= rhi::EFragmentFormat::R8_U_Int;
		samplesNumTexturesDef.usage			= lib::Flags(rhi::ETextureUsage::StorageTexture, rhi::ETextureUsage::SampledTexture, rhi::ETextureUsage::TransferDest);
#if !SPT_RELEASE
		lib::AddFlag(samplesNumTexturesDef.usage, rhi::ETextureUsage::TransferSource);
#endif // !SPT_RELEASE
		const rdr::RendererResourceName accumulatedSamplesTextureName = RENDERER_RESOURCE_NAME_FORMATTED("{}: AccumulatedSamplesNum Texture", m_debugName.AsString());
		m_accumulatedSamplesNum			= rdr::ResourcesManager::CreateTextureView(accumulatedSamplesTextureName, samplesNumTexturesDef, rhi::EMemoryUsage::GPUOnly);
		m_accumulatedSamplesNumHistory	= rdr::ResourcesManager::CreateTextureView(accumulatedSamplesTextureName, samplesNumTexturesDef, rhi::EMemoryUsage::GPUOnly);

		rhi::TextureDefinition temporalMomentsTexturesDef;
		temporalMomentsTexturesDef.resolution = denoisedTexture->GetResolution2D();
		temporalMomentsTexturesDef.format     = rhi::EFragmentFormat::RG16_S_Float;
		temporalMomentsTexturesDef.usage      = lib::Flags(rhi::ETextureUsage::StorageTexture, rhi::ETextureUsage::SampledTexture, rhi::ETextureUsage::TransferDest);
#if !SPT_RELEASE
		lib::AddFlag(temporalMomentsTexturesDef.usage, rhi::ETextureUsage::TransferSource);
#endif // !SPT_RELEASE
		m_temporalMoments        = rdr::ResourcesManager::CreateTextureView(RENDERER_RESOURCE_NAME_FORMATTED("{}: Temporal Moments Texture", m_debugName.AsString()), temporalMomentsTexturesDef, rhi::EMemoryUsage::GPUOnly);
		m_temporalMomentsHistory = rdr::ResourcesManager::CreateTextureView(RENDERER_RESOURCE_NAME_FORMATTED("{}: Temporal Moments Texture", m_debugName.AsString()), temporalMomentsTexturesDef, rhi::EMemoryUsage::GPUOnly);

		const rg::RGTextureViewHandle accumulatedSamplesNumHistory = graphBuilder.AcquireExternalTextureView(m_accumulatedSamplesNumHistory);
		const rg::RGTextureViewHandle temporalMomentsHistory       = graphBuilder.AcquireExternalTextureView(m_temporalMomentsHistory);

		graphBuilder.ClearTexture(RG_DEBUG_NAME_FORMATTED("{}: Clear Accumulated History Num", m_debugName.AsString()), accumulatedSamplesNumHistory, rhi::ClearColor(0u, 0u, 0u, 0u));
		graphBuilder.ClearTexture(RG_DEBUG_NAME_FORMATTED("{}: Clear Temporal Moments History", m_debugName.AsString()), temporalMomentsHistory, rhi::ClearColor(0.f, 0.f, 0.f, 0.f));

		m_hasValidHistory = false;
	}
}

void Denoiser::DenoiseImpl(rg::RenderGraphBuilder& graphBuilder, rg::RGTextureViewHandle denoisedTexture, const Params& params)
{
	SPT_PROFILER_FUNCTION();

	SPT_RG_DIAGNOSTICS_SCOPE(graphBuilder, "Visibility Denoiser");

	const math::Vector2u resolution = denoisedTexture->GetResolution2D();

	const rg::RGTextureViewHandle historyTexture               = graphBuilder.AcquireExternalTextureView(m_historyTexture);
	const rg::RGTextureViewHandle accumulatedSamplesNum        = graphBuilder.AcquireExternalTextureView(m_accumulatedSamplesNum);
	const rg::RGTextureViewHandle accumulatedSamplesNumHistory = graphBuilder.AcquireExternalTextureView(m_accumulatedSamplesNumHistory);
	const rg::RGTextureViewHandle temporalMoments              = graphBuilder.AcquireExternalTextureView(m_temporalMoments);
	const rg::RGTextureViewHandle temporalMomentsHistory       = graphBuilder.AcquireExternalTextureView(m_temporalMomentsHistory);

	denoising::DenoiserBaseParams denoiserParams(params.renderView);
	denoiserParams.name                = m_debugName;
	denoiserParams.historyDepthTexture = params.historyDepthTexture;
	denoiserParams.currentDepthTexture = params.currentDepthTexture;
	denoiserParams.motionTexture       = params.motionTexture;
	denoiserParams.normalsTexture      = params.normalsTexture;

	moments::VisibilityMomentsParameters visibilityMomentsParams;
	visibilityMomentsParams.debugName   = m_debugName;
	visibilityMomentsParams.dataTexture = denoisedTexture;
	const rg::RGTextureViewHandle spatialMomentsTexture = moments::ComputeMoments(graphBuilder, visibilityMomentsParams);

	const rg::RGTextureViewHandle varianceTexture = graphBuilder.CreateTextureView(RG_DEBUG_NAME_FORMATTED("{}: Variance Texture", m_debugName.AsString()),
																				   rg::TextureDef(resolution, rhi::EFragmentFormat::R16_S_Float));

	if (m_hasValidHistory && params.useTemporalFilter)
	{
		temporal_accumulation::TemporalFilterParams temporalParams(denoiserParams);
		temporalParams.currentTexture                      = denoisedTexture;
		temporalParams.historyTexture                      = historyTexture;
		temporalParams.currentFrameDefaultWeight           = params.currentFrameDefaultWeight;
		temporalParams.accumulatedFramesMaxCount           = params.accumulatedFramesMaxCount;
		temporalParams.spatialMomentsTexture               = spatialMomentsTexture;
		temporalParams.accumulatedSamplesNumTexture        = accumulatedSamplesNum;
		temporalParams.accumulatedSamplesNumHistoryTexture = accumulatedSamplesNumHistory;
		temporalParams.temporalMomentsTexture              = temporalMoments;
		temporalParams.temporalMomentsHistoryTexture       = temporalMomentsHistory;
		temporalParams.varianceTexture                     = varianceTexture;
		temporal_accumulation::ApplyTemporalFilter(graphBuilder, temporalParams);
	}
	else
	{
		graphBuilder.ClearTexture(RG_DEBUG_NAME_FORMATTED("{}: Clear Variance Texture", m_debugName.AsString()), varianceTexture, rhi::ClearColor(0.f, 0.f, 0.f, 0.f));
		graphBuilder.ClearTexture(RG_DEBUG_NAME_FORMATTED("{}: Clear Accumulated Samples Num Texture", m_debugName.AsString()), accumulatedSamplesNum, rhi::ClearColor(0, 0, 0, 0));
	}

	ApplySpatialFilters(graphBuilder, denoiserParams, denoisedTexture, historyTexture, varianceTexture);

	m_hasValidHistory = true;

	std::swap(m_accumulatedSamplesNum, m_accumulatedSamplesNumHistory);
	std::swap(m_temporalMoments, m_temporalMomentsHistory);
}

void Denoiser::ApplySpatialFilters(rg::RenderGraphBuilder& graphBuilder, const denoising::DenoiserBaseParams& params, rg::RGTextureViewHandle denoisedTexture, rg::RGTextureViewHandle historyTexture, rg::RGTextureViewHandle varianceTexture)
{
	SPT_PROFILER_FUNCTION();

	const rg::TextureDef& textureDef = denoisedTexture->GetTextureDefinition();
	const rg::RGTextureViewHandle tempTexture = graphBuilder.CreateTextureView(RG_DEBUG_NAME_FORMATTED("{}: Denoise Temp Texture", m_debugName.AsString()), textureDef);

	spatial::SpatialATrousFilterParams spatialParams = params;
	spatialParams.varianceTexture = varianceTexture;

	Uint32 iterationIdx = 0;
	spatial::ApplyATrousFilter(graphBuilder, spatialParams, denoisedTexture, historyTexture, iterationIdx++);
	spatial::ApplyATrousFilter(graphBuilder, spatialParams, historyTexture, tempTexture, iterationIdx++);
	spatial::ApplyATrousFilter(graphBuilder, spatialParams, tempTexture, denoisedTexture, iterationIdx);
}

} // spt::rsc::dir_shadows_denoiser
