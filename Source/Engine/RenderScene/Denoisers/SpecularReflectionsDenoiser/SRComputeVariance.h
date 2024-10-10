#pragma once

#include "SculptorCoreTypes.h"
#include "RGResources/RGResourceHandles.h"
#include "RenderGraphTypes.h"


namespace spt::rg
{
class RenderGraphBuilder;
} // spt::rg


namespace spt::rsc
{

class RenderView;


namespace sr_denoiser
{

struct TemporalVarianceParams
{
	explicit TemporalVarianceParams(const RenderView& inRenderView)
		: renderView(inRenderView)
	{ }

	const RenderView&        renderView;
	rg::RenderGraphDebugName debugName;
	rg::RGTextureViewHandle  momentsTexture;
	rg::RGTextureViewHandle  accumulatedSamplesNumTexture;
	rg::RGTextureViewHandle  normalsTexture;
	rg::RGTextureViewHandle  depthTexture;
	rg::RGTextureViewHandle  luminanceTexture;

	rg::RGTextureViewHandle  outputVarianceTexture;
};


rg::RGTextureViewHandle CreateVarianceTexture(rg::RenderGraphBuilder& graphBuilder, math::Vector2u resolution);


void ComputeTemporalVariance(rg::RenderGraphBuilder& graphBuilder, const TemporalVarianceParams& params);

} // sr_denoiser

} // spt::rsc
