#pragma once

#include "SculptorCoreTypes.h"
#include "RenderSceneMacros.h"
#include "DenoiserTypes.h"


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


namespace dir_shadows_denoiser
{

struct DirShadowsDenoiserParams : public denoising::DenoiserBaseParams
{
	DirShadowsDenoiserParams(const RenderView& inRenderView)
		: denoising::DenoiserBaseParams(inRenderView)
		, enableTemporalFilter(false)
	{
	}

	Bool					enableTemporalFilter;
};


RENDER_SCENE_API void Denoise(rg::RenderGraphBuilder& graphBuilder, const DirShadowsDenoiserParams& params);

} // dir_shadows_denoiser

} // spt::rsc
