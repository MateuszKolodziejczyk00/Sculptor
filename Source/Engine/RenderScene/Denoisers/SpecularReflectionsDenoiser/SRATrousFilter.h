#pragma once

#include "Denoisers/DenoiserTypes.h"


namespace spt::rg
{
class RenderGraphBuilder;
} // spt::rg


namespace spt::rsc
{

namespace sr_denoiser
{

struct SRATrousFilterParams : public denoising::DenoiserGeometryParams
{
	using denoising::DenoiserGeometryParams::DenoiserGeometryParams;

	SRATrousFilterParams(const denoising::DenoiserGeometryParams& inParams)
		: denoising::DenoiserGeometryParams(inParams)
	{  }

	rg::RGTextureViewHandle stdDevTexture;
	rg::RGTextureViewHandle specularColorRoughnessTexture;
};

void ApplyATrousFilter(rg::RenderGraphBuilder& graphBuilder, const SRATrousFilterParams& params, rg::RGTextureViewHandle input, rg::RGTextureViewHandle output, Uint32 iterationIdx);

} // sr_denoiser

} // spt::rsc
