#pragma once

#include "RenderSceneMacros.h"
#include "RGResources/RGResourceHandles.h"
#include "RenderGraphTypes.h"


namespace spt::rg
{
class RenderGraphBuilder;
} // spt::rg


namespace spt::rsc::denoising::variance
{

struct VarianceParameters
{
	rg::RenderGraphDebugName	debugName;

	rg::RGTextureViewHandle		dataTexture;
};


RENDER_SCENE_API rg::RGTextureViewHandle ComputeVariance(rg::RenderGraphBuilder& graphBuilder, const VarianceParameters& params);

} // spt::rsc::denoising::variance
