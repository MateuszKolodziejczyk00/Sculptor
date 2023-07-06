#pragma once

#include "SculptorCoreTypes.h"
#include "RenderSceneMacros.h"
#include "RGResources/RGResourceHandles.h"


namespace spt::rg
{
class RenderGraphBuilder;
} // spt::rg


namespace spt::rdr
{
class TextureView;
} // spt::rdr


namespace spt::rsc
{

class RenderView;


namespace ao_denoiser
{

struct DenoiserParams
{
	DenoiserParams(const RenderView& inRenderView)
		: renderView(inRenderView)
	{ }

	const RenderView& renderView;

	rg::RGTextureViewHandle historyDepthTexture;
	rg::RGTextureViewHandle currentDepthTexture;
	
	rg::RGTextureViewHandle motionTexture;

	rg::RGTextureViewHandle geometryNormalsTexture;

	rg::RGTextureViewHandle currentAOTexture;
	rg::RGTextureViewHandle historyAOTexture;

	Bool hasValidHistory;
};


RENDER_SCENE_API void Denoise(rg::RenderGraphBuilder& graphBuilder, const DenoiserParams& params);

} // ao_denoiser

} // spt::rsc