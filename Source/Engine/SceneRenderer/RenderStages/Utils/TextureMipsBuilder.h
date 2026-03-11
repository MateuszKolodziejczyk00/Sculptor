#pragma once

#include "SculptorCoreTypes.h"
#include "RGResources/RGResourceHandles.h"
#include "RHICore/RHITextureTypes.h"


namespace spt::rg
{
class RenderGraphBuilder;
} // spt::rg


namespace spt::rsc
{

class ViewRenderingSpec;

namespace MipsBuilder
{

void BuildTextureMips(rg::RenderGraphBuilder& graphBuilder, rg::RGTextureHandle texture, Uint32 sourceMipLevel, Uint32 mipLevelsNum);

} // MipsBuilder

} // spt::rsc