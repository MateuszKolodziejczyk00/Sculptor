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

namespace material_depth_renderer
{

namespace constants
{
static constexpr rhi::EFragmentFormat materialDepthFormat = rhi::EFragmentFormat::D16_UN_Float;
} // constants

rg::RGTextureViewHandle CreateMaterialDepthTexture(rg::RenderGraphBuilder& graphBuilder, const math::Vector2u& resolution);

struct MaterialDepthParameters
{
	rg::RGTextureViewHandle visibilityTexture;
	rg::RGBufferViewHandle  visibleMeshlets;
};

void RenderMaterialDepth(rg::RenderGraphBuilder& graphBuilder, const MaterialDepthParameters& parameters, rg::RGTextureViewHandle materialDepth);

} // material_depth_renderer

} // spt::rsc