#pragma once

#include "SculptorCoreTypes.h"
#include "RGResources/RGResourceHandles.h"


namespace spt::rg
{
class RenderGraphBuilder;
} // spt::rg


namespace spt::rsc
{

namespace ray_binner
{

rg::RGTextureViewHandle ExecuteRayBinning(rg::RenderGraphBuilder& graphBuilder, rg::RGTextureViewHandle rayDirectionsTexture);

} // ray_binner

} // spt::rsc