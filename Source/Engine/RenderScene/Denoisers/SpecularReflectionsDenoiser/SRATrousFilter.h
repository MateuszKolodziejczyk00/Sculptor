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

	rg::RGTextureViewHandle specularHistoryLengthTexture;
};


struct SRATrousPass
{
	struct SHTextures
	{
		rg::RGTextureViewHandle diffuseY_SH2;
		rg::RGTextureViewHandle specularY_SH2;
		rg::RGTextureViewHandle diffSpecCoCg;
	};

	struct Textures
	{
		rg::RGTextureViewHandle specular;
		rg::RGTextureViewHandle diffuse;
	};

	const SHTextures* inSHTextures = nullptr;

	const Textures*   outTextures   = nullptr;
	const SHTextures* outSHTextures = nullptr;

	rg::RGTextureViewHandle inVariance;
	rg::RGTextureViewHandle outVariance;

	Bool enableWideFilter = true;

	Uint32 iterationIdx = 0u;
};


void ApplyATrousFilter(rg::RenderGraphBuilder& graphBuilder, const SRATrousFilterParams& filterParams, const SRATrousPass& passParams);


} // sr_denoiser

} // spt::rsc
