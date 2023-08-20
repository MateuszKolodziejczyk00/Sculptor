#include "DirectionalLightShadowsDenoiser.h"
#include "Filters/TemporalFilter.h"
#include "Filters/SpatialATrousFilter.h"
#include "RenderGraphBuilder.h"


namespace spt::rsc::dir_shadows_denoiser
{

void ApplySpatialFilters(rg::RenderGraphBuilder& graphBuilder, const DirShadowsDenoiserParams& params)
{
	SPT_PROFILER_FUNCTION();

	const rhi::TextureDefinition& textureDef = params.currentTexture->GetTextureDefinition();
	const rg::RGTextureViewHandle tempTexture = graphBuilder.CreateTextureView(RG_DEBUG_NAME("Dir Shadows Denoise Temp Texture"), textureDef, rhi::EMemoryUsage::GPUOnly);

	Uint32 iterationIdx = 0;
	denoising::filters::spatial::ApplyATrousFilter(graphBuilder, params, params.currentTexture, tempTexture, iterationIdx++);
	denoising::filters::spatial::ApplyATrousFilter(graphBuilder, params, tempTexture, params.currentTexture, iterationIdx++);
}

void Denoise(rg::RenderGraphBuilder& graphBuilder, const DirShadowsDenoiserParams& params)
{
	SPT_PROFILER_FUNCTION();

	if (params.HasValidHistory() && params.enableTemporalFilter)
	{
		denoising::filters::temporal::ApplyTemporalFilter(graphBuilder, params);
	}
	
	graphBuilder.CopyTexture(RG_DEBUG_NAME("Copy Shadow Mask"), params.currentTexture, math::Vector3i::Zero(), params.historyTexture, math::Vector3i::Zero(), params.historyTexture->GetResolution());

	ApplySpatialFilters(graphBuilder, params);
}

} // spt::rsc::dir_shadows_denoiser
