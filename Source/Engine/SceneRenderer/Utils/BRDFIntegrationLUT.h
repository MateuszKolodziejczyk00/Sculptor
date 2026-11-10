#pragma once

#include "SceneRendererMacros.h"
#include "SculptorCoreTypes.h"
#include "RGResources/RGResourceHandles.h"


namespace spt::rg
{
class RenderGraphBuilder;
} // spt::rg


namespace spt::rsc
{

SCENE_RENDERER_API void GenerateBRDFIntegrationLUT(rg::RenderGraphBuilder& graphBuilder, rg::RGTextureViewHandle lut);

} // spt::rsc