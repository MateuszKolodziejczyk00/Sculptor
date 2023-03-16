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

namespace HiZ
{

rg::RGTextureViewHandle CreateHierarchicalZ(rg::RenderGraphBuilder& graphBuilder, ViewRenderingSpec& viewSpec, rg::RGTextureViewHandle depthTexture, rhi::EFragmentFormat depthFormat);

} // HiZ

} // spt::rsc