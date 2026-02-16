#pragma once

#include "SculptorCoreTypes.h"
#include "GeometryTypes.h"
#include "RGResources/RGResourceHandles.h"
#include "RenderGraphBuilder.h"


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
} // spt::rsc


namespace spt::rsc::gp
{

struct GeometryPassParams
{
	ViewRenderingSpec& viewSpec;

	const GeometryPassDataCollection& geometryPassData;

	rg::RGTextureViewHandle historyHiZ;
	rg::RGTextureViewHandle depth;
	rg::RGTextureViewHandle hiZ;
};


enum class EGeometryPass
{
	// Renders geometry that was visible in the previous frame
	// Performs all culling operations + occlusion culling with last frame's HiZ
	// Outputs occluded batch elements that are in frustum for DisoccludedGeometryPass pass
	VisibleGeometryPass,
	// Renders batch elements that were disoccluded in the current frame
	// Performs occlusion culling with current frame's HiZ
	DisoccludedGeometryPass,
	// Renders meshlets that were disoccluded in the current frame
	// Note that this is different from DisoccludedGeometryPass pass, because it doesn't process whole batch elements
	DisoccludedMeshletsPass,
	Num
};


struct RenderPassDefinition : public rg::RGRenderPassDefinition
{
	explicit RenderPassDefinition(math::Vector2u inResolution)
		: rg::RGRenderPassDefinition(math::Vector2i(0, 0), inResolution)
		, resolution(inResolution)
		
	{}

	math::Vector2u                              resolution;
	lib::MTHandle<rg::RGDescriptorSetStateBase> perPassDS;
};


class GeometryRenderingPipeline
{
public:

	virtual void                 Prologue(rg::RenderGraphBuilder& graphBuilder, const GeometryPassParams& geometryPassParams) const { }
	virtual RenderPassDefinition CreateRenderPassDefinition(rg::RenderGraphBuilder& graphBuilder, const GeometryPassParams& geometryPassParams, EGeometryPass pass) const = 0;
	virtual rdr::PipelineStateID CreatePipelineForBatch(const GeometryPassParams& geometryPassParams, const GeometryBatch& batch, EGeometryPass pass) const = 0;
};


#define GEOMETRY_PIPELINE_SHADERS \
	TASK_SHADER("Sculptor/GeometryRendering/GeometryRenderingPipeline.hlsl", GeometryPipeline_TS);     \
	MESH_SHADER("Sculptor/GeometryRendering/GeometryRenderingPipeline.hlsl", GeometryPipeline_MS);     \
	FRAGMENT_SHADER("Sculptor/GeometryRendering/GeometryRenderingPipeline.hlsl", GeometryPipeline_FS); \


BEGIN_SHADER_STRUCT(GeometryPipelinePermutation)
	SHADER_STRUCT_FIELD(GeometryBatchPermutation, BATCH)
	SHADER_STRUCT_FIELD(Int32,                    GEOMETRY_PASS_IDX)
	SHADER_STRUCT_FIELD(lib::HashedString,        GEOMETRY_PIPELINE_SHADER)
END_SHADER_STRUCT();


void ExecutePipeline(rg::RenderGraphBuilder& graphBuilder, const GeometryPassParams& geometryPassParams, GeometryRenderingPipeline& pipeline);

} // spt::rsc::gp
