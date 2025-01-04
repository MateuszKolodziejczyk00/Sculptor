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

struct ClampHistoryParams
{
	explicit ClampHistoryParams(const RenderView& inRenderView)
		: renderView(inRenderView)
	{ }

	const RenderView&        renderView;
	rg::RenderGraphDebugName debugName;

	rg::RGTextureViewHandle  specularHistoryLengthTexture;
	rg::RGTextureViewHandle  diffuseHistoryLengthTexture;

	rg::RGTextureViewHandle  depthTexture;
	rg::RGTextureViewHandle  normalsTexture;
	rg::RGTextureViewHandle  roughnessTexture;

	rg::RGTextureViewHandle  specularTexture;
	rg::RGTextureViewHandle  fastHistorySpecularTexture;

	rg::RGTextureViewHandle  diffuseTexture;
	rg::RGTextureViewHandle  fastHistoryDiffuseTexture;
};


void ClampHistory(rg::RenderGraphBuilder& graphBuilder, const ClampHistoryParams& params);

} // sr_denoiser

} // spt::rsc
