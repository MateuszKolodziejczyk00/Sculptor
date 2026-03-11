#pragma once

#include "SculptorCoreTypes.h"
#include "RGResources/RGResourceHandles.h"


namespace spt::rg
{
class RenderGraphBuilder;
} // spt::rg


namespace spt::rsc
{

class RenderView;


rg::RGTextureViewHandle ExecuteLinearizeDepth(rg::RenderGraphBuilder& graphBuilder, const RenderView& renderView, rg::RGTextureViewHandle depth);

} // spt::rsc
