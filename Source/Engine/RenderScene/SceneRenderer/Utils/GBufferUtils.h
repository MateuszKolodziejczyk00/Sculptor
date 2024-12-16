#pragma once

#include "SculptorCoreTypes.h"
#include "SceneRenderer/SceneRenderingTypes.h"


namespace spt::rg
{
class RenderGraphBuilder;
} // spt::rg


namespace spt::rsc
{

namespace gbuffer_utils
{

namespace oct_normals
{

void GenerateOctahedronNormals(rg::RenderGraphBuilder& graphBuilder, const GBuffer& gBuffer, rg::RGTextureViewHandle outputTexture);

} // oct_normals

} // gbuffer_utils

} // spt::rsc
