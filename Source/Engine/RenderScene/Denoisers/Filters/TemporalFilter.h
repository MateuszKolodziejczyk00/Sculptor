#pragma once

#include "RenderSceneMacros.h"
#include "Denoisers/DenoiserTypes.h"


namespace spt::rg
{
class RenderGraphBuilder;
} // spt::rg


namespace spt::rsc::denoising::filters::temporal
{

struct TemporalFilterParams : public denoising::DenoiserBaseParams
{
	using denoising::DenoiserBaseParams::DenoiserBaseParams;

	TemporalFilterParams(const denoising::DenoiserBaseParams& inParams)
		: denoising::DenoiserBaseParams(inParams)
	{  }

	Real32 currentFrameMinWeight = 0.1f;
	Real32 currentFrameMaxWeight = 0.35f;
	
	Real32 varianceHistoryClampMultiplier = 8.f;
	
	Real32 minHistoryClampExtent = 0.15f;

	rg::RGTextureViewHandle varianceTexture;
};


RENDER_SCENE_API void ApplyTemporalFilter(rg::RenderGraphBuilder& graphBuilder, const TemporalFilterParams& params);

} // spt::rsc::denoising::filters::temporal
