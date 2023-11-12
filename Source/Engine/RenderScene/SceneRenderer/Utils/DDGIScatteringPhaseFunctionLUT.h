#pragma once

#include "SculptorCoreTypes.h"
#include "RGResources/RGResourceHandles.h"


namespace spt::rdr
{
class TextureView;
} // spt::rdr



namespace spt::rg
{
class RenderGraphBuilder;
} // spt::rg


namespace spt::rsc
{

class DDGIScatteringPhaseFunctionLUT
{
public:

	static DDGIScatteringPhaseFunctionLUT& Get();

	rg::RGTextureViewHandle GetLUT(rg::RenderGraphBuilder& graphBuilder);

private:

	DDGIScatteringPhaseFunctionLUT();

	rg::RGTextureViewHandle CreateLUT(rg::RenderGraphBuilder& graphBuilder);

	lib::SharedPtr<rdr::TextureView> m_lut;
};

} // spt::rsc
