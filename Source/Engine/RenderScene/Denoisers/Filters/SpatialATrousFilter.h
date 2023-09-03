#pragma once

#include "RenderSceneMacros.h"
#include "Denoisers/DenoiserTypes.h"


namespace spt::rg
{
class RenderGraphBuilder;
} // spt::rg

namespace spt::rsc::denoising::filters::spatial
{

struct SpatialATrousFilterParams : public denoising::DenoiserGeometryParams
{
	using denoising::DenoiserGeometryParams::DenoiserGeometryParams;

	SpatialATrousFilterParams(const denoising::DenoiserGeometryParams& inParams)
		: denoising::DenoiserGeometryParams(inParams)
	{  }

	rg::RGTextureViewHandle varianceTexture;
};

RENDER_SCENE_API void ApplyATrousFilter(rg::RenderGraphBuilder& graphBuilder, const SpatialATrousFilterParams& params, rg::RGTextureViewHandle input, rg::RGTextureViewHandle output, Uint32 iterationIdx);

} // spt::rsc::denoising::filters::spatial
