#pragma once

#include "SculptorCoreTypes.h"
#include "GeometryPipeline.h"


namespace spt::rsc
{

struct SceneRendererInterface;


struct VisPassParams 
{
	gp::GeometryPassParams geometryPassParams;

	rg::RGTextureViewHandle visibilityTexture;
};


struct VisPassResult
{
	rg::RGBufferViewHandle visibleMeshlets;
};


class VisPassRenderer
{
public:

	VisPassRenderer();

	VisPassResult RenderVisibility(rg::RenderGraphBuilder& graphBuilder, const SceneRendererInterface& rendererInterface, const VisPassParams& visPassParams);

private:

	lib::SharedPtr<rdr::Buffer> m_visibleMeshlets;
	lib::SharedPtr<rdr::Buffer> m_visibleMeshletsCount;
};

} // spt::rsc
