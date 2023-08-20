#pragma once

#include "RenderSceneMacros.h"
#include "Denoisers/DenoiserTypes.h"


namespace spt::rg
{
class RenderGraphBuilder;
} // spt::rg

namespace spt::rsc::denoising::filters::spatial
{

using SpatialATrousFilterParams = denoising::DenoiserGeometryParams;

RENDER_SCENE_API void ApplyATrousFilter(rg::RenderGraphBuilder& graphBuilder, const SpatialATrousFilterParams& params, rg::RGTextureViewHandle input, rg::RGTextureViewHandle output, Uint32 iterationIdx);

} // spt::rsc::denoising::filters::spatial
