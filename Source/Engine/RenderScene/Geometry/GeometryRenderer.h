#pragma once

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


struct GeometryPassResult
{
	rg::RGBufferViewHandle visibleMeshlets;
};


struct GeometryOITPassParams
{
	ViewRenderingSpec& viewSpec;

	const GeometryPassDataCollection& geometryPassData;

	rg::RGTextureViewHandle depth;
	rg::RGTextureViewHandle hiZ;
};


struct GeometryOITResult
{
	rg::RGBufferViewHandle visibleMeshlets;
};


class GeometryRenderer
{
public:

	GeometryRenderer();

	GeometryPassResult RenderVisibility(rg::RenderGraphBuilder& graphBuilder, const VisPassParams& visPassParams);
	GeometryOITResult  RenderTransparentGeometry(rg::RenderGraphBuilder& graphBuilder, const GeometryOITPassParams& oitPassParams);

private:

	lib::SharedPtr<rdr::Buffer> m_visibleMeshlets;
	lib::SharedPtr<rdr::Buffer> m_visibleMeshletsCount;
};

} // spt::rsc