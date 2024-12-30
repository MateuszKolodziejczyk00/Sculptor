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

	rg::RGTextureViewHandle linearDepthTexture;
	rg::RGTextureViewHandle roughnessTexture;
	rg::RGTextureViewHandle reprojectionConfidenceTexture;
	rg::RGTextureViewHandle historyFramesNumTexture;

	Bool enableDetailPreservation = false;
};


struct SRATrousPass
{
	rg::RGTextureViewHandle inSpecularLuminance;
	rg::RGTextureViewHandle inDiffuseLuminance;

	rg::RGTextureViewHandle outSpecularLuminance;
	rg::RGTextureViewHandle outDiffuseLuminance;

	rg::RGTextureViewHandle inVariance;
	rg::RGTextureViewHandle outVariance;

	Uint32 iterationIdx = 0u;
};


void ApplyATrousFilter(rg::RenderGraphBuilder& graphBuilder, const SRATrousFilterParams& filterParams, const SRATrousPass& passParams);


} // sr_denoiser

} // spt::rsc
