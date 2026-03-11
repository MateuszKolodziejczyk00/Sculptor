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

class BRDFIntegrationLUT
{
public:

	static BRDFIntegrationLUT& Get();

	rg::RGTextureViewHandle GetLUT(rg::RenderGraphBuilder& graphBuilder);

private:

	BRDFIntegrationLUT();

	rg::RGTextureViewHandle CreateLUT(rg::RenderGraphBuilder& graphBuilder);

	lib::SharedPtr<rdr::TextureView> m_lut;
};

} // spt::rsc
