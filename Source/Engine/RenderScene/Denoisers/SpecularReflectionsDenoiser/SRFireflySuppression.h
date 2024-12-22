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

	rg::RGTextureViewHandle  inSpecularHitDistTexture;
	rg::RGTextureViewHandle  outSpecularHitDistTexture;

	rg::RGTextureViewHandle  inDiffuseHitDistTexture;
	rg::RGTextureViewHandle  outDiffuseHitDistTexture;
};


void SuppressFireflies(rg::RenderGraphBuilder& graphBuilder, const FireflySuppressionParams& params);

} // sr_denoiser

} // spt::rsc
