#pragma once

#include "RenderSceneMacros.h"
#include "Denoisers/DenoiserTypes.h"


namespace spt::rg
{
class RenderGraphBuilder;
} // spt::rg


namespace spt::rsc::visibility_denoiser::temporal_accumulation
{

struct TemporalFilterParams : public denoising::DenoiserBaseParams
{
	using denoising::DenoiserBaseParams::DenoiserBaseParams;

	explicit TemporalFilterParams(const denoising::DenoiserBaseParams& inParams)
		: denoising::DenoiserBaseParams(inParams)
	{  }

	Real32 currentFrameDefaultWeight = 0.16f;

	Real32 accumulatedFramesMaxCount = 12.f;

	rg::RGTextureViewHandle currentTexture;
	rg::RGTextureViewHandle historyTexture;

	rg::RGTextureViewHandle spatialMomentsTexture;

	rg::RGTextureViewHandle accumulatedSamplesNumTexture;
	rg::RGTextureViewHandle accumulatedSamplesNumHistoryTexture;

	rg::RGTextureViewHandle temporalMomentsTexture;
	rg::RGTextureViewHandle temporalMomentsHistoryTexture;
	
	rg::RGTextureViewHandle varianceTexture;
};


RENDER_SCENE_API void ApplyTemporalFilter(rg::RenderGraphBuilder& graphBuilder, const TemporalFilterParams& params);

} // spt::rsc::visibility_denoiser::temporal_accumulation
