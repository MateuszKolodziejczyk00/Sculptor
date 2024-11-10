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


struct VarianceEstimationParams
{
	explicit VarianceEstimationParams(const RenderView& inRenderView)
		: renderView(inRenderView)
	{ }

	const RenderView&        renderView;
	rg::RenderGraphDebugName debugName;
	rg::RGTextureViewHandle  varianceTexture;
	rg::RGTextureViewHandle  accumulatedSamplesNumTexture;
	rg::RGTextureViewHandle  normalsTexture;
	rg::RGTextureViewHandle  depthTexture;
	rg::RGTextureViewHandle  roughnessTexture;

	rg::RGTextureViewHandle  tempVarianceTexture;

	rg::RGTextureViewHandle  outputEstimatedVarianceTexture;

	Real32 gaussianBlurSigma = 3.0f;
};


void EstimateVariance(rg::RenderGraphBuilder& graphBuilder, const VarianceEstimationParams& params);

} // sr_denoiser

} // spt::rsc
