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

struct StdDevParams
{
	explicit StdDevParams(const RenderView& inRenderView)
		: renderView(inRenderView)
	{ }

	const RenderView&        renderView;
	rg::RenderGraphDebugName debugName;
	rg::RGTextureViewHandle  momentsTexture;
	rg::RGTextureViewHandle  accumulatedSamplesNumTexture;
	rg::RGTextureViewHandle  normalsTexture;
	rg::RGTextureViewHandle  depthTexture;
	rg::RGTextureViewHandle  luminanceTexture;
};


rg::RGTextureViewHandle ComputeStandardDeviation(rg::RenderGraphBuilder& graphBuilder, const StdDevParams& params);

} // sr_denoiser

} // spt::rsc
