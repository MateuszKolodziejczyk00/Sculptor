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

class RenderViewDS;


namespace upsampler
{

struct DepthBasedUpsampleParams
{
	rg::RenderGraphDebugName debugName;

	rg::RGTextureViewHandle depth;
	rg::RGTextureViewHandle depthHalfRes;

	lib::MTHandle<RenderViewDS> renderViewDS;
};

rg::RGTextureViewHandle DepthBasedUpsample(rg::RenderGraphBuilder& graphBuilder, rg::RGTextureViewHandle texture, const DepthBasedUpsampleParams& params);

} // upsampler

} // spt::rsc