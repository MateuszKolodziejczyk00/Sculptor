#pragma once

#include "SculptorCoreTypes.h"
#include "RGResources/RGResourceHandles.h"


namespace spt::rg
{
class RenderGraphBuilder;
} // spt::rg


namespace spt::rsc
{

class RenderScene;
class ViewRenderingSpec;


namespace stochastic_di
{

struct StochasticDIParams
{
	rg::RGTextureViewHandle outputLuminance;
};


void RenderDI(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const StochasticDIParams& diParams);

} // stochastic_di

} // spt::rsc