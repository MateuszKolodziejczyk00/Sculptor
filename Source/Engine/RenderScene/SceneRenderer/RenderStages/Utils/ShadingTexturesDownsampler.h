#pragma once

#include "SculptorCoreTypes.h"
#include "RGResources/RGResourceHandles.h"


namespace spt::rg
{
class RenderGraphBuilder;
} // spt::rg


namespace spt::rsc
{

class RenderView;


struct DownsampledShadingTexturesParams
{
	// Input
	rg::RGTextureViewHandle depth;
	rg::RGTextureViewHandle roughness;
	rg::RGTextureViewHandle tangentFrame;


	// Output
	rg::RGTextureViewHandle roughnessHalfRes;
	rg::RGTextureViewHandle shadingNormalsHalfRes;
};

void DownsampleShadingTextures(rg::RenderGraphBuilder& graphBuilder, const RenderView& renderView, const DownsampledShadingTexturesParams& params);

} // spt::rsc