#include "AmbientOcclusionDenoiser.h"
#include "Filters/TemporalFilter.h"
#include "Filters/SpatialATrousFilter.h"
#include "RenderGraphBuilder.h"
#include "Utils/VariancePass.h"


namespace spt::rsc
{

namespace ao_denoiser
{

static void ApplySpatialFilter(rg::RenderGraphBuilder& graphBuilder, const AODenoiserParams& params, rg::RGTextureViewHandle varianceTexture)
{
	SPT_PROFILER_FUNCTION();

	const rhi::TextureDefinition& textureDef = params.currentTexture->GetTextureDefinition();
	const rg::RGTextureViewHandle tempTexture = graphBuilder.CreateTextureView(RG_DEBUG_NAME("AO Denoise Temp Texture"), textureDef, rhi::EMemoryUsage::GPUOnly);

	denoising::filters::spatial::SpatialATrousFilterParams spatialParams = params;
	spatialParams.varianceTexture = varianceTexture;

	Uint32 iterationIdx = 0;
	denoising::filters::spatial::ApplyATrousFilter(graphBuilder, spatialParams, params.currentTexture, params.historyTexture, iterationIdx++);
	denoising::filters::spatial::ApplyATrousFilter(graphBuilder, spatialParams, params.historyTexture, params.currentTexture, iterationIdx++);
	denoising::filters::spatial::ApplyATrousFilter(graphBuilder, spatialParams, params.currentTexture, tempTexture, iterationIdx++);
	denoising::filters::spatial::ApplyATrousFilter(graphBuilder, spatialParams, tempTexture, params.currentTexture, iterationIdx++);
}

void Denoise(rg::RenderGraphBuilder& graphBuilder, const AODenoiserParams& params)
{
	SPT_PROFILER_FUNCTION();

	denoising::variance::VarianceParameters varianceParams;
	varianceParams.debugName	= params.name;
	varianceParams.dataTexture	= params.currentTexture;
	const rg::RGTextureViewHandle varianceTexture = denoising::variance::ComputeVariance(graphBuilder, varianceParams);

	if (params.enableTemporalFilter)
	{
		// Don't use variance for temporal filter as ambient occlusion has huge variance in most cases
		// and it will decrease temporal filter quality even in static scenes
		denoising::filters::temporal::ApplyTemporalFilter(graphBuilder, params);
	}
	ApplySpatialFilter(graphBuilder, params, varianceTexture);
}

} // ao_denoiser

} // spt::rsc
