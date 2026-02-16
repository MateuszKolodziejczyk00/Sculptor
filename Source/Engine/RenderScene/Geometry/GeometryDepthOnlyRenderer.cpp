#include "GeometryDepthOnlyRenderer.h"
#include "View/ViewRenderingSpec.h"


namespace spt::rsc::depth_only
{

GRAPHICS_PSO(GeometryDepthOnlyPSO)
{
	GEOMETRY_PIPELINE_SHADERS;

	PERMUTATION_DOMAIN(gp::GeometryPipelinePermutation)
};


class DepthOnlyRenderingPipeline : public gp::GeometryRenderingPipeline
{
public:

	DepthOnlyRenderingPipeline() = default;

	// Begin GeometryRenderingPipeline overrides
	virtual gp::RenderPassDefinition CreateRenderPassDefinition(rg::RenderGraphBuilder& graphBuilder, const gp::GeometryPassParams& geometryPassParams, gp::EGeometryPass pass) const override;
	virtual rdr::PipelineStateID     CreatePipelineForBatch(const gp::GeometryPassParams& geometryPassParams, const GeometryBatch& batch, gp::EGeometryPass pass) const override;
	// End GeometryRenderingPipeline overrides
};


gp::RenderPassDefinition DepthOnlyRenderingPipeline::CreateRenderPassDefinition(rg::RenderGraphBuilder& graphBuilder, const gp::GeometryPassParams& geometryPassParams, gp::EGeometryPass pass) const
{
	const RenderView& renderView = geometryPassParams.viewSpec.GetRenderView();
	const math::Vector2u resolution = renderView.GetRenderingRes();

	gp::RenderPassDefinition passDef(resolution);

	const rhi::ERTLoadOperation renderTargetLoadOp = pass == gp::EGeometryPass::VisibleGeometryPass ? rhi::ERTLoadOperation::Clear : rhi::ERTLoadOperation::Load;

	rg::RGRenderTargetDef depthRTDef;
	depthRTDef.textureView    = geometryPassParams.depth;
	depthRTDef.loadOperation  = renderTargetLoadOp;
	depthRTDef.storeOperation = rhi::ERTStoreOperation::Store;
	depthRTDef.clearColor     = rhi::ClearColor(0.f);
	passDef.SetDepthRenderTarget(depthRTDef);

	return passDef;
}

rdr::PipelineStateID DepthOnlyRenderingPipeline::CreatePipelineForBatch(const gp::GeometryPassParams& geometryPassParams, const GeometryBatch& batch, gp::EGeometryPass pass) const
{
	SPT_PROFILER_FUNCTION();

	const rhi::GraphicsPipelineDefinition pipelineDef
	{
		.primitiveTopology = rhi::EPrimitiveTopology::TriangleList,
		.rasterizationDefinition =
		{
			.cullMode = batch.permutation.DOUBLE_SIDED ? rhi::ECullMode::None : rhi::ECullMode::Back,
		},
		.renderTargetsDefinition =
		{
			.depthRTDefinition = rhi::DepthRenderTargetDefinition
			{
				.format = geometryPassParams.depth->GetFormat(),
				.depthCompareOp = rhi::ECompareOp::Greater,
			}
		}
	};

	gp::GeometryPipelinePermutation permutation;
	permutation.BATCH                    = batch.permutation;
	permutation.GEOMETRY_PASS_IDX        = static_cast<Int32>(pass);
	permutation.GEOMETRY_PIPELINE_SHADER = "\"GeometryRendering/GeometryDepthOnly.hlsli\"";

	return GeometryDepthOnlyPSO::GetPermutation(pipelineDef, permutation);
}


void RenderDepthOnly(rg::RenderGraphBuilder& graphBuilder, const gp::GeometryPassParams& geometryPassParams)
{
	SPT_PROFILER_FUNCTION();

	SPT_RG_DIAGNOSTICS_SCOPE(graphBuilder, "Geometry Rendering (Depth Only)");

	DepthOnlyRenderingPipeline pipeline;

	gp::ExecutePipeline(graphBuilder, geometryPassParams, pipeline);
}

} // spt::rsc::depth_only
