#pragma once

#include "RenderSceneMacros.h"
#include "SculptorCoreTypes.h"
#include "GeometryTypes.h"
#include "RGResources/RGResourceHandles.h"


namespace spt::rdr
{
class Buffer;
} // spt::rdr


namespace spt::rg
{
class RenderGraphBuilder;
} // spt::rg


namespace spt::rsc
{

class ViewRenderingSpec;
struct GeometryPassDataCollection;
struct GeometryBatch;


struct VisPassParams
{
	ViewRenderingSpec& viewSpec;

	const GeometryPassDataCollection& geometryPassData;

	rg::RGTextureViewHandle historyHiZ;
	rg::RGTextureViewHandle depth;
	rg::RGTextureViewHandle hiZ;

	rg::RGTextureViewHandle visibilityTexture;

};


class GeometryRenderer
{
public:

	GeometryRenderer();

	void RenderVisibility(rg::RenderGraphBuilder& graphBuilder, const VisPassParams& visPassParams);

private:

	rdr::PipelineStateID CreatePipelineForBatch(const GeometryBatch& batch) const;

	lib::SharedPtr<rdr::Buffer> m_visibleMeshlets;
	lib::SharedPtr<rdr::Buffer> m_visibleMeshletsCount;
};

} // spt::rsc