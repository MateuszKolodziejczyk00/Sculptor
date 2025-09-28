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

struct FireflySuppressionParams
{
	FireflySuppressionParams() = default;

	rg::RenderGraphDebugName debugName;

	rg::RGTextureViewHandle  normal;

	rg::RGTextureViewHandle  inSpecularY_SH2;
	rg::RGTextureViewHandle  inDiffuseY_SH2;
	rg::RGTextureViewHandle  inDiffSpecCoCg;

	rg::RGTextureViewHandle  outSpecularY_SH2;
	rg::RGTextureViewHandle  outDiffuseY_SH2;
	rg::RGTextureViewHandle  outDiffSpecCoCg;
};


void SuppressFireflies(rg::RenderGraphBuilder& graphBuilder, const FireflySuppressionParams& params);

} // sr_denoiser

} // spt::rsc
