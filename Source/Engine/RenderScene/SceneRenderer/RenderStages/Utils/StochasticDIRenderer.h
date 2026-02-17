#pragma once

#include "SculptorCoreTypes.h"
#include "RGResources/RGResourceHandles.h"
#include "Denoisers/SpecularReflectionsDenoiser/SRDenoiser.h"


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

	Renderer();
	
	void Render(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const StochasticDIParams& diParams);

private:

	math::Vector2u              m_historyResolution;
	lib::SharedPtr<rdr::Buffer> m_historyReservoirs;

	sr_denoiser::Denoiser m_denoiser;
};

} // stochastic_di

} // spt::rsc
