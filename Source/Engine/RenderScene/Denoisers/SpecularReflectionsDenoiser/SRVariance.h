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
	rg::RGTextureViewHandle  accumulatedSamplesNumTexture;
	rg::RGTextureViewHandle  normalsTexture;
	rg::RGTextureViewHandle  depthTexture;

	rg::RGTextureViewHandle  specularMomentsTexture;
	rg::RGTextureViewHandle  specularTexture;

	rg::RGTextureViewHandle  diffuseMomentsTexture;
	rg::RGTextureViewHandle  diffuseTexture;

	rg::RGTextureViewHandle  outVarianceTexture;
};


rg::RGTextureViewHandle CreateVarianceTexture(rg::RenderGraphBuilder& graphBuilder, const rg::RenderGraphDebugName& name, math::Vector2u resolution);
rg::RGTextureViewHandle CreateVarianceEstimationTexture(rg::RenderGraphBuilder& graphBuilder, const rg::RenderGraphDebugName& name, math::Vector2u resolution);


void ComputeTemporalVariance(rg::RenderGraphBuilder& graphBuilder, const TemporalVarianceParams& params);


struct VarianceEstimationParams
{
	explicit VarianceEstimationParams(const RenderView& inRenderView)
		: renderView(inRenderView)
	{ }

	const RenderView&        renderView;
	rg::RenderGraphDebugName debugName;

	rg::RGTextureViewHandle  accumulatedSamplesNumTexture;
	rg::RGTextureViewHandle  normalsTexture;
	rg::RGTextureViewHandle  depthTexture;
	rg::RGTextureViewHandle  roughnessTexture;

	rg::RGTextureViewHandle  specularVarianceTexture;
	rg::RGTextureViewHandle  diffuseVarianceTexture;

	rg::RGTextureViewHandle  outEstimatedVarianceTexture;

	Real32 gaussianBlurSigma = 3.0f;
};


void EstimateVariance(rg::RenderGraphBuilder& graphBuilder, const VarianceEstimationParams& params);

} // sr_denoiser

} // spt::rsc
