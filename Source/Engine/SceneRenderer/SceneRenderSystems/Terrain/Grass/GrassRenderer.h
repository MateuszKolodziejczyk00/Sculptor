#pragma once

#include "SculptorCoreTypes.h"
#include "SceneRendererTypes.h"
#include "GrassTypes.h"


namespace spt::rsc
{

class ViewRenderingSpec;


namespace grass_renderer
{

GrassBlades GenerateGrassBlades(rg::RenderGraphBuilder& graphBuilder, const ViewRenderingSpec& viewSpec, const GrassFieldDefinition& def);


struct GrassVisibilityRenderParams
{
	const ViewRenderingSpec& viewSpec;
	const GrassBlades&       grassBlades;

	rg::RGTextureViewHandle depthTexture;
	rg::RGTextureViewHandle visibilityTexture;
};

void RenderGrassVisibility(rg::RenderGraphBuilder& graphBuilder, const GrassVisibilityRenderParams& params);

} // grass_renderer

} // spt::rsc
