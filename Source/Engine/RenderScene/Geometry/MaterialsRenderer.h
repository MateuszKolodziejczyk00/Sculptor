#pragma once

#include "SculptorCoreTypes.h"
#include "GeometryTypes.h"
#include "RGResources/RGResourceHandles.h"


namespace spt::rg
{
class RenderGraphBuilder;
} // spt::rg


namespace spt::rsc
{

class RenderScene;
class ViewRenderingSpec;


namespace materials_renderer
{

struct MaterialsPassParams
{
	MaterialsPassParams(const ViewRenderingSpec& inViewSpec, const GeometryPassDataCollection& inGeometryPassData)
		: viewSpec(inViewSpec)
		, geometryPassData(inGeometryPassData)
	{
	}

	const ViewRenderingSpec& viewSpec;
	const GeometryPassDataCollection& geometryPassData;

	Uint32 materialDepthTileSize;

	rg::RGTextureViewHandle materialDepthTexture;
	rg::RGTextureViewHandle materialDepthTileTexture;
	rg::RGTextureViewHandle depthTexture;

	rg::RGTextureViewHandle visibilityTexture;

	rg::RGBufferViewHandle visibleMeshletsBuffer;
};

void ExecuteMaterialsPass(rg::RenderGraphBuilder& graphBuilder, const MaterialsPassParams& passParams);

} // materials_renderer

} // spt::rsc
