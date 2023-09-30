#pragma once

#include "RenderSceneMacros.h"
#include "RGResources/RGResourceHandles.h"
#include "RenderGraphTypes.h"


namespace spt::rg
{
class RenderGraphBuilder;
} // spt::rg


namespace spt::rsc::visibility_denoiser::moments
{

struct VisibilityMomentsParameters
{
	rg::RenderGraphDebugName	debugName;

	rg::RGTextureViewHandle		dataTexture;
};


RENDER_SCENE_API rg::RGTextureViewHandle ComputeMoments(rg::RenderGraphBuilder& graphBuilder, const VisibilityMomentsParameters& params);

} // spt::rsc::visibility_denoiser::moments