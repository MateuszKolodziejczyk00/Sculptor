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
	rg::RGTextureViewHandle specularColorRoughness;
	rg::RGTextureViewHandle shadingNormals;

	// Output
	rg::RGTextureViewHandle specularColorRoughnessHalfRes;
	rg::RGTextureViewHandle shadingNormalsHalfRes;
};

void DownsampleShadingTextures(rg::RenderGraphBuilder& graphBuilder, const RenderView& renderView, const DownsampledShadingTexturesParams& params);

} // spt::rsc