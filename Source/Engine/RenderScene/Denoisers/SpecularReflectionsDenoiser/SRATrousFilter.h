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
	rg::RGTextureViewHandle inputLuminance;
	rg::RGTextureViewHandle outputLuminance;

	Uint32 iterationIdx = 0u;

	SRATrousPass& operator++()
	{
		std::swap(inputLuminance, outputLuminance);
		++iterationIdx;
		return *this;
	}
};


void ApplyATrousFilter(rg::RenderGraphBuilder& graphBuilder, const SRATrousFilterParams& filterParams, const SRATrousPass& passParams, rg::RGTextureViewHandle inputVariance, rg::RGTextureViewHandle outputVariance);


} // sr_denoiser

} // spt::rsc
