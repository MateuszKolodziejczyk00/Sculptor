#pragma once

#include "SculptorCoreTypes.h"
#include "RGResources/RGResourceHandles.h"
#include "RHICore/RHITextureTypes.h"


namespace spt::rg
{
class RenderGraphBuilder;
} // spt::rg


namespace spt::rsc
{

class ViewRenderingSpec;

namespace bilateral_grid
{

math::Vector2u GetGridTileSize();

struct BilateralGridInfo
{
	rg::RGTextureViewHandle bilateralGrid;
	rg::RGTextureViewHandle downsampledLogLuminance;

	math::Vector2u tilesSize;
};

namespace builder
{

struct BilateralGridParams
{
	explicit BilateralGridParams(ViewRenderingSpec& viewSpec, rg::RGTextureViewHandle inputTexture)
		: viewSpec(viewSpec)
		, inputTexture(inputTexture)
	{
	}

	ViewRenderingSpec& viewSpec;
	rg::RGTextureViewHandle inputTexture;

	Real32 minLogLuminance   = 0.f;
	Real32 logLuminanceRange = 0.f;
};


BilateralGridInfo RenderBilateralGrid(rg::RenderGraphBuilder& graphBuilder, const BilateralGridParams& parameters);

} // builder

} // bilateral_grid_renderer

} // spt::rsc