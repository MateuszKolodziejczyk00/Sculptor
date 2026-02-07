#pragma once

#include "SculptorCoreTypes.h"
#include "RGResources/RGResourceHandles.h"


namespace spt::rg
{
class RenderGraphBuilder;
} // spt::rg


namespace spt::rdr
{
class Buffer;
} // spt::rdr


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


class Renderer
{
public:
	
	void Render(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const StochasticDIParams& diParams);

private:

	math::Vector2u              m_historyResolution;
	lib::SharedPtr<rdr::Buffer> m_historyReservoirs;
};

} // stochastic_di

} // spt::rsc
