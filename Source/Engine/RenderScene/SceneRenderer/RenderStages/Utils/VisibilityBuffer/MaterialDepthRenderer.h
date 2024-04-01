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

rg::RGTextureViewHandle CreateMaterialDepthTexture(rg::RenderGraphBuilder& graphBuilder, const math::Vector2u& resolution);

struct MaterialDepthParameters
{
	rg::RGTextureViewHandle visibilityTexture;
	rg::RGBufferViewHandle  visibleMeshlets;
};

void RenderMaterialDepth(rg::RenderGraphBuilder& graphBuilder, const MaterialDepthParameters& parameters, rg::RGTextureViewHandle materialDepth);

} // material_depth_renderer


namespace material_depth_tiles_renderer
{

Uint32 GetMaterialDepthTileSize();

rg::RGTextureViewHandle CreateMaterialDepthTilesTexture(rg::RenderGraphBuilder& graphBuilder, const math::Vector2u& resolution);

void RenderMaterialDepthTiles(rg::RenderGraphBuilder& graphBuilder, rg::RGTextureViewHandle materialDepth, rg::RGTextureViewHandle materialDepthTiles);

} // material_depth_tiles_renderer

} // spt::rsc