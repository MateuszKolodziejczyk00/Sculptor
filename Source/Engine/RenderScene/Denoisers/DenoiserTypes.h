#pragma once

#include "SculptorCoreTypes.h"
#include "RGResources/RGResourceHandles.h"


namespace spt::rsc
{
class RenderView;
} // spt::rsc


namespace spt::rsc::denoising
{

struct DenoiserBaseParams
{
	explicit DenoiserBaseParams(const RenderView& inRenderView)
		: renderView(inRenderView)
	{ }

	Bool HasValidHistory() const
	{
		return historyDepthTexture.IsValid() && historyTexture.IsValid();
	}

	const RenderView& renderView;

	rg::RGTextureViewHandle historyDepthTexture;
	rg::RGTextureViewHandle currentDepthTexture;

	rg::RGTextureViewHandle motionTexture;

	rg::RGTextureViewHandle geometryNormalsTexture;

	rg::RGTextureViewHandle currentTexture;
	rg::RGTextureViewHandle historyTexture;
};


struct DenoiserGeometryParams
{
	explicit DenoiserGeometryParams(const RenderView& inRenderView)
		: renderView(inRenderView)
	{ }
	
	DenoiserGeometryParams(const DenoiserBaseParams& denoiserParams)
		: renderView(denoiserParams.renderView)
		, depthTexture(denoiserParams.currentDepthTexture)
		, geometryNormalsTexture(denoiserParams.geometryNormalsTexture)
	{ }

	const RenderView& renderView;

	rg::RGTextureViewHandle depthTexture;

	rg::RGTextureViewHandle geometryNormalsTexture;
};

}  // spt::rsc::denoising