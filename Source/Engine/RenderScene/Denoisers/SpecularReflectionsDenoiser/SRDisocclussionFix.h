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

	rg::RGTextureViewHandle  roughnessTexture;

	rg::RGTextureViewHandle  diffuseTexture;
	rg::RGTextureViewHandle  outputDiffuseTexture;

	rg::RGTextureViewHandle  specularTexture;
	rg::RGTextureViewHandle  outputSpecularTexture;
};


void DisocclusionFix(rg::RenderGraphBuilder& graphBuilder, const DisocclusionFixParams& params);

} // sr_denoiser

} // spt::rsc
