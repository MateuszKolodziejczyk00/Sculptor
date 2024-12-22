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
	rg::RGTextureViewHandle inSpecularVariance;
	rg::RGTextureViewHandle inDiffuseVariance;

	rg::RGTextureViewHandle outSpecularLuminance;
	rg::RGTextureViewHandle outDiffuseLuminance;
	rg::RGTextureViewHandle outSpecularVariance;
	rg::RGTextureViewHandle outDiffuseVariance;

	Uint32 iterationIdx = 0u;

	SRATrousPass& operator++()
	{
		std::swap(inSpecularLuminance, outSpecularLuminance);
		std::swap(inDiffuseLuminance, outDiffuseLuminance);
		std::swap(inSpecularVariance, outSpecularVariance);
		std::swap(inDiffuseVariance, outDiffuseVariance);
		++iterationIdx;
		return *this;
	}
};


void ApplyATrousFilter(rg::RenderGraphBuilder& graphBuilder, const SRATrousFilterParams& filterParams, const SRATrousPass& passParams);


} // sr_denoiser

} // spt::rsc
