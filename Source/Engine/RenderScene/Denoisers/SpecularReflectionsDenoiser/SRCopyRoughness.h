#pragma once

#include "SculptorCoreTypes.h"
#include "RGResources/RGResourceTypes.h"


namespace spt::rg
{
class RenderGraphBuilder;
} // spt::rg


namespace spt::rsc::sr_denoiser
{

void CopyRoughness(rg::RenderGraphBuilder& graphBuilder, rg::RGTextureViewHandle specularColorRoughnessTexture, rg::RGTextureViewHandle destRoughnessTexture);

} // spt::rsc::sr_denoiser
