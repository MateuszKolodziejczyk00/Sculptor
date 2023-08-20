#pragma once

#include "RenderSceneMacros.h"
#include "Denoisers/DenoiserTypes.h"


namespace spt::rg
{
class RenderGraphBuilder;
} // spt::rg


namespace spt::rsc::denoising::filters::temporal
{

using TemporalFilterParams = denoising::DenoiserBaseParams;


RENDER_SCENE_API void ApplyTemporalFilter(rg::RenderGraphBuilder& graphBuilder, const TemporalFilterParams& params);

} // spt::rsc::denoising::filters::temporal
