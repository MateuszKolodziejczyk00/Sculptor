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

	gp::GeometryPipelineExecutor* CreatePipelineExecutor(rg::RenderGraphBuilder& graphBuilder, const SceneRendererInterface& rendererInterface, const VisPassParams& visPassParams) const;
	VisPassResult                 FinishPipelineExecution(rg::RenderGraphBuilder& graphBuilder, gp::GeometryPipelineExecutor* executor) const;

private:

	lib::SharedPtr<rdr::Buffer> m_visibleMeshlets;
	lib::SharedPtr<rdr::Buffer> m_visibleMeshletsCount;
};

} // spt::rsc
