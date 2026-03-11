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

struct HiZSizeInfo
{
	math::Vector2u resolution = {};
	Uint32         mipLevels  = 0u;
};

HiZSizeInfo ComputeHiZSizeInfo(const math::Vector2u& resolution);

void CreateHierarchicalZ(rg::RenderGraphBuilder& graphBuilder, rg::RGTextureViewHandle depthTexture, rg::RGTextureHandle hiZ);

} // HiZ

} // spt::rsc