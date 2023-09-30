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
		const rdr::RendererResourceName accumulatedSamplesTextureName = RENDERER_RESOURCE_NAME_FORMATTED("{}: AccumulatedSamplesNum Texture", m_debugName.AsString());
		m_accumulatedSamplesNum			= rdr::ResourcesManager::CreateTextureView(accumulatedSamplesTextureName, samplesNumTexturesDef, rhi::EMemoryUsage::GPUOnly);
		m_accumulatedSamplesNumHistory	= rdr::ResourcesManager::CreateTextureView(accumulatedSamplesTextureName, samplesNumTexturesDef, rhi::EMemoryUsage::GPUOnly);

		m_hasValidHistory = false;

		const rg::RGTextureViewHandle accumulatedSamplesNumHistory = graphBuilder.AcquireExternalTextureView(m_accumulatedSamplesNumHistory);

		graphBuilder.ClearTexture(RG_DEBUG_NAME_FORMATTED("{}: Clear Accumulated History Num", m_debugName.AsString()), accumulatedSamplesNumHistory, rhi::ClearColor(0u, 0u, 0u, 0u));
	}
}

void Denoiser::DenoiseImpl(rg::RenderGraphBuilder& graphBuilder, rg::RGTextureViewHandle denoisedTexture, const Params& params)
{
	SPT_PROFILER_FUNCTION();

	const rg::RGTextureViewHandle historyTexture = graphBuilder.AcquireExternalTextureView(m_historyTexture);
	const rg::RGTextureViewHandle accumulatedSamplesNum = graphBuilder.AcquireExternalTextureView(m_accumulatedSamplesNum);
	const rg::RGTextureViewHandle accumulatedSamplesNumHistory = graphBuilder.AcquireExternalTextureView(m_accumulatedSamplesNumHistory);

	denoising::DenoiserBaseParams denoiserParams(params.renderView);
	denoiserParams.name						= m_debugName;
	denoiserParams.historyDepthTexture		= params.historyDepthTexture;
	denoiserParams.currentDepthTexture		= params.currentDepthTexture;
	denoiserParams.motionTexture			= params.motionTexture;
	denoiserParams.geometryNormalsTexture	= params.geometryNormalsTexture;
	denoiserParams.currentTexture			= denoisedTexture;
	denoiserParams.historyTexture			= historyTexture;

	moments::VisibilityMomentsParameters visibilityMomentsParams;
	visibilityMomentsParams.debugName		= m_debugName;
	visibilityMomentsParams.dataTexture	= denoisedTexture;
	const rg::RGTextureViewHandle momentsTexture = moments::ComputeMoments(graphBuilder, visibilityMomentsParams);

	if (m_hasValidHistory && params.useTemporalFilter)
	{
		temporal_accumulation::TemporalFilterParams temporalParams(denoiserParams);
		temporalParams.momentsTexture						= momentsTexture;
		temporalParams.accumulatedSamplesNumTexture			= accumulatedSamplesNum;
		temporalParams.accumulatedSamplesNumHistoryTexture	= accumulatedSamplesNumHistory;
		temporalParams.linearAndNearestSamplesMaxDepthDiff	= params.reprojectionLinearAndNearestSamplesMaxDepthDiff;
		temporal_accumulation::ApplyTemporalFilter(graphBuilder, temporalParams);
	}

	graphBuilder.CopyFullTexture(RG_DEBUG_NAME_FORMATTED("{}: Copy History Texture", m_debugName.AsString()), denoisedTexture, historyTexture);

	ApplySpatialFilters(graphBuilder, denoiserParams, momentsTexture);

	m_hasValidHistory = true;

	std::swap(m_accumulatedSamplesNum, m_accumulatedSamplesNumHistory);
}

void Denoiser::ApplySpatialFilters(rg::RenderGraphBuilder& graphBuilder, const denoising::DenoiserBaseParams& params, rg::RGTextureViewHandle momentsTexture)
{
	SPT_PROFILER_FUNCTION();

	const rg::TextureDef& textureDef = params.currentTexture->GetTextureDefinition();
	const rg::RGTextureViewHandle tempTexture = graphBuilder.CreateTextureView(RG_DEBUG_NAME_FORMATTED("{}: Denoise Temp Texture", m_debugName.AsString()), textureDef);
	const rg::RGTextureViewHandle tempTexture2 = graphBuilder.CreateTextureView(RG_DEBUG_NAME_FORMATTED("{}: Denoise Temp Texture (2)", m_debugName.AsString()), textureDef);

	spatial::SpatialATrousFilterParams spatialParams = params;
	spatialParams.momentsTexture = momentsTexture;

	Uint32 iterationIdx = 0;
	spatial::ApplyATrousFilter(graphBuilder, spatialParams, params.currentTexture, tempTexture, iterationIdx++);
	spatial::ApplyATrousFilter(graphBuilder, spatialParams, tempTexture, tempTexture2, iterationIdx++);
	spatial::ApplyATrousFilter(graphBuilder, spatialParams, tempTexture2, params.currentTexture, iterationIdx++);
}

} // spt::rsc::dir_shadows_denoiser
