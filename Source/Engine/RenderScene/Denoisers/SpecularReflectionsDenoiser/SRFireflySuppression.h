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
	rg::RGTextureViewHandle  inputLuminanceHitDisTexture;
	rg::RGTextureViewHandle  outputLuminanceHitDisTexture;
};


void SuppressFireflies(rg::RenderGraphBuilder& graphBuilder, const FireflySuppressionParams& params);

} // sr_denoiser

} // spt::rsc
