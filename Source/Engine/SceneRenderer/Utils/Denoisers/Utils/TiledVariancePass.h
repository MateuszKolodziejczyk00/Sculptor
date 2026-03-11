#pragma once

#include "RenderSceneMacros.h"
#include "RGResources/RGResourceHandles.h"
#include "RenderGraphTypes.h"


namespace spt::rg
{
class RenderGraphBuilder;
} // spt::rg


namespace spt::rsc::denoising::tiled_variance
{

struct TiledVarianceParameters
{
	rg::RenderGraphDebugName	debugName;

	rg::RGTextureViewHandle		dataTexture;
};


RENDER_SCENE_API rg::RGTextureViewHandle ComputeVariance(rg::RenderGraphBuilder& graphBuilder, const TiledVarianceParameters& params);

} // spt::rsc::denoising::variance
