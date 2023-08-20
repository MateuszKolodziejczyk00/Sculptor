#include "AmbientOcclusionDenoiser.h"
#include "Filters/TemporalFilter.h"
#include "Filters/SpatialATrousFilter.h"
#include "RenderGraphBuilder.h"


namespace spt::rsc
{

namespace ao_denoiser
{

static void ApplySpatialFilter(rg::RenderGraphBuilder& graphBuilder, const AODenoiserParams& params)
{
	SPT_PROFILER_FUNCTION();

	const rhi::TextureDefinition& textureDef = params.currentTexture->GetTextureDefinition();
	const rg::RGTextureViewHandle tempTexture = graphBuilder.CreateTextureView(RG_DEBUG_NAME("AO Denoise Temp Texture"), textureDef, rhi::EMemoryUsage::GPUOnly);

	Uint32 iterationIdx = 0;
	denoising::filters::spatial::ApplyATrousFilter(graphBuilder, params, params.currentTexture, params.historyTexture, iterationIdx++);
	denoising::filters::spatial::ApplyATrousFilter(graphBuilder, params, params.historyTexture, params.currentTexture, iterationIdx++);
	denoising::filters::spatial::ApplyATrousFilter(graphBuilder, params, params.currentTexture, tempTexture, iterationIdx++);
	denoising::filters::spatial::ApplyATrousFilter(graphBuilder, params, tempTexture, params.currentTexture, iterationIdx++);
}

void Denoise(rg::RenderGraphBuilder& graphBuilder, const AODenoiserParams& params)
{
	SPT_PROFILER_FUNCTION();

	if (params.enableTemporalFilter)
	{
		denoising::filters::temporal::ApplyTemporalFilter(graphBuilder, params);
	}
	ApplySpatialFilter(graphBuilder, params);
}

} // ao_denoiser

} // spt::rsc
