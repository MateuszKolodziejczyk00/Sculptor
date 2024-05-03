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

struct DisocclusionFixParams
{
	explicit DisocclusionFixParams(const RenderView& inRenderView)
		: renderView(inRenderView)
	{ }

	const RenderView&        renderView;
	rg::RenderGraphDebugName debugName;
	rg::RGTextureViewHandle  accumulatedSamplesNumTexture;
	rg::RGTextureViewHandle  normalsTexture;
	rg::RGTextureViewHandle  depthTexture;
	rg::RGTextureViewHandle  luminanceTexture;
	rg::RGTextureViewHandle  outputLuminanceTexture;
	rg::RGTextureViewHandle  roughnessTexture;
};


void DisocclusionFix(rg::RenderGraphBuilder& graphBuilder, const DisocclusionFixParams& params);

} // sr_denoiser

} // spt::rsc
