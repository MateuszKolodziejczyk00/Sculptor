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

	rg::RGTextureViewHandle  specularHistoryLengthTexture;
	rg::RGTextureViewHandle  diffuseHistoryLengthTexture;

	rg::RGTextureViewHandle  normalsTexture;
	rg::RGTextureViewHandle  depthTexture;

	rg::RGTextureViewHandle  roughnessTexture;

	rg::RGTextureViewHandle inSpecularY_SH2;
	rg::RGTextureViewHandle inDiffuseY_SH2;
	rg::RGTextureViewHandle inDiffSpecCoCg;

	rg::RGTextureViewHandle outSpecularY_SH2;
	rg::RGTextureViewHandle outDiffuseY_SH2;
	rg::RGTextureViewHandle outDiffSpecCoCg;
};


void DisocclusionFix(rg::RenderGraphBuilder& graphBuilder, const DisocclusionFixParams& params);

} // sr_denoiser

} // spt::rsc
