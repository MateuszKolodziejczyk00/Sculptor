#include "DirectionalLightShadowsDenoiser.h"
#include "Filters/TemporalFilter.h"
#include "Filters/SpatialATrousFilter.h"
#include "RenderGraphBuilder.h"
#include "Utils/VariancePass.h"


namespace spt::rsc::dir_shadows_denoiser
{

void ApplySpatialFilters(rg::RenderGraphBuilder& graphBuilder, const DirShadowsDenoiserParams& params, rg::RGTextureViewHandle varianceTexture)
{
	SPT_PROFILER_FUNCTION();

	const rhi::TextureDefinition& textureDef = params.currentTexture->GetTextureDefinition();
	const rg::RGTextureViewHandle tempTexture = graphBuilder.CreateTextureView(RG_DEBUG_NAME("Dir Shadows Denoise Temp Texture"), textureDef, rhi::EMemoryUsage::GPUOnly);

	denoising::filters::spatial::SpatialATrousFilterParams spatialParams = params;
	spatialParams.varianceTexture = varianceTexture;

	Uint32 iterationIdx = 0;
	denoising::filters::spatial::ApplyATrousFilter(graphBuilder, spatialParams, params.currentTexture, tempTexture, iterationIdx++);
	denoising::filters::spatial::ApplyATrousFilter(graphBuilder, spatialParams, tempTexture, params.currentTexture, iterationIdx++);
}

void Denoise(rg::RenderGraphBuilder& graphBuilder, const DirShadowsDenoiserParams& params)
{
	SPT_PROFILER_FUNCTION();

	denoising::variance::VarianceParameters varianceParams;
	varianceParams.debugName	= params.name;
	varianceParams.dataTexture	= params.currentTexture;
	const rg::RGTextureViewHandle varianceTexture = denoising::variance::ComputeVariance(graphBuilder, varianceParams);

	if (params.HasValidHistory() && params.enableTemporalFilter)
	{
		denoising::filters::temporal::TemporalFilterParams temporalParams = params;
		temporalParams.varianceTexture = varianceTexture;
		denoising::filters::temporal::ApplyTemporalFilter(graphBuilder, temporalParams);
	}
	
	graphBuilder.CopyTexture(RG_DEBUG_NAME("Copy Shadow Mask"), params.currentTexture, math::Vector3i::Zero(), params.historyTexture, math::Vector3i::Zero(), params.historyTexture->GetResolution());

	ApplySpatialFilters(graphBuilder, params, varianceTexture);
}

} // spt::rsc::dir_shadows_denoiser