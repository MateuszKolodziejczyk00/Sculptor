#include "GeometryVisPassRenderer.h"
#include "View/ViewRenderingSpec.h"


namespace spt::rsc
{

namespace vis_pass
{

DS_BEGIN(VisBufferRenderingDS, rg::RGDescriptorSetState<VisBufferRenderingDS>)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<GPUVisibleMeshlet>), u_visibleMeshlets)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<Uint32>),            u_visibleMeshletsCount)
DS_END();


GRAPHICS_PSO(GeometryVisBufferPSO)
{
	GEOMETRY_PIPELINE_SHADERS;

	PERMUTATION_DOMAIN(gp::GeometryPipelinePermutation)
};


class VisibilityBufferRenderingPipeline : public gp::GeometryRenderingPipeline
{
public:

	VisibilityBufferRenderingPipeline(rg::RGTextureViewHandle visibilityTexture, rg::RGBufferViewHandle visibleMeshlets, rg::RGBufferViewHandle visibleMeshletsCount)
		: m_visibilityTexture(visibilityTexture)
		, m_visibleMeshlets(visibleMeshlets)
		, m_visibleMeshletsCount(visibleMeshletsCount)
	{ }

	// Begin GeometryRenderingPipeline overrides
	virtual void                     Prologue(rg::RenderGraphBuilder& graphBuilder, const gp::GeometryPassParams& geometryPassParams) const override;
	virtual gp::RenderPassDefinition CreateRenderPassDefinition(rg::RenderGraphBuilder& graphBuilder, const gp::GeometryPassParams& geometryPassParams, gp::EGeometryPass pass) const override;
	virtual rdr::PipelineStateID     CreatePipelineForBatch(const gp::GeometryPassParams& geometryPassParams, const GeometryBatch& batch, gp::EGeometryPass pass) const override;
	// End GeometryRenderingPipeline overrides

private:

	rg::RGTextureViewHandle m_visibilityTexture;
	rg::RGBufferViewHandle m_visibleMeshlets;
	rg::RGBufferViewHandle m_visibleMeshletsCount;
};

void VisibilityBufferRenderingPipeline::Prologue(rg::RenderGraphBuilder& graphBuilder, const gp::GeometryPassParams& geometryPassParams) const
{
	graphBuilder.MemZeroBuffer(m_visibleMeshletsCount);
}

gp::RenderPassDefinition VisibilityBufferRenderingPipeline::CreateRenderPassDefinition(rg::RenderGraphBuilder& graphBuilder, const gp::GeometryPassParams& geometryPassParams, gp::EGeometryPass pass) const
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

	if (m_visibilityTexture.IsValid())
	{
		rg::RGRenderTargetDef visibilityRTDef;
		visibilityRTDef.textureView = m_visibilityTexture;
		visibilityRTDef.loadOperation = renderTargetLoadOp;
		visibilityRTDef.storeOperation = rhi::ERTStoreOperation::Store;
		visibilityRTDef.clearColor = rhi::ClearColor(idxNone<Uint32>, 0u, 0u, 0u);
		passDef.AddColorRenderTarget(visibilityRTDef);
	}

	lib::MTHandle<VisBufferRenderingDS> perPassDS = graphBuilder.CreateDescriptorSet<VisBufferRenderingDS>(RENDERER_RESOURCE_NAME("VisBufferRenderingDS"));
	perPassDS->u_visibleMeshlets      = m_visibleMeshlets;
	perPassDS->u_visibleMeshletsCount = m_visibleMeshletsCount;
	passDef.perPassDS = perPassDS;

	return passDef;
}

rdr::PipelineStateID VisibilityBufferRenderingPipeline::CreatePipelineForBatch(const gp::GeometryPassParams& geometryPassParams, const GeometryBatch& batch, gp::EGeometryPass pass) const
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
			.colorRTsDefinition =
			{
				rhi::ColorRenderTargetDefinition
				{
					.format         = m_visibilityTexture->GetFormat(),
					.colorBlendType = rhi::ERenderTargetBlendType::Disabled,
					.alphaBlendType = rhi::ERenderTargetBlendType::Disabled,
				}
			},
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
	permutation.GEOMETRY_PIPELINE_SHADER = "\"GeometryRendering/GeometryVisibilityBuffer.hlsli\"";

	return GeometryVisBufferPSO::GetPermutation(pipelineDef, permutation);
}

} // vis_pass


VisPassRenderer::VisPassRenderer()
{
	rhi::BufferDefinition countBufferDef;
	countBufferDef.size  = sizeof(Uint32);
	countBufferDef.usage = lib::Flags(rhi::EBufferUsage::Storage, rhi::EBufferUsage::TransferDst);
	m_visibleMeshletsCount = rdr::ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME("Visible Meshlets Count"), countBufferDef, rhi::EMemoryUsage::GPUOnly);

	// TODO handle resizing, use smaller default size
	const Uint64 defaultSize = sizeof(GPUVisibleMeshlet) * 1024 * 128;
	
	rhi::BufferDefinition meshletsBufferDef;
	meshletsBufferDef.size  = defaultSize;
	meshletsBufferDef.usage = lib::Flags(rhi::EBufferUsage::Storage, rhi::EBufferUsage::TransferDst);
	m_visibleMeshlets = rdr::ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME("Visible Meshlets"), meshletsBufferDef, rhi::EMemoryUsage::GPUOnly);
}

VisPassResult VisPassRenderer::RenderVisibility(rg::RenderGraphBuilder& graphBuilder, const VisPassParams& visPassParams)
{
	SPT_PROFILER_FUNCTION();

	SPT_RG_DIAGNOSTICS_SCOPE(graphBuilder, "Visibliity Buffer");

	SPT_CHECK(visPassParams.geometryPassParams.depth.IsValid());
	SPT_CHECK(visPassParams.geometryPassParams.hiZ.IsValid());
	SPT_CHECK(visPassParams.visibilityTexture.IsValid());

	const rg::RGBufferViewHandle visibleMeshlets      = graphBuilder.AcquireExternalBufferView(m_visibleMeshlets->GetFullView());
	const rg::RGBufferViewHandle visibleMeshletsCount = graphBuilder.AcquireExternalBufferView(m_visibleMeshletsCount->GetFullView());

	vis_pass::VisibilityBufferRenderingPipeline pipeline(visPassParams.visibilityTexture, visibleMeshlets, visibleMeshletsCount);

	gp::ExecutePipeline(graphBuilder, visPassParams.geometryPassParams, pipeline);

	VisPassResult result;
	result.visibleMeshlets = visibleMeshlets;

	return result;
}

} // spt::rsc
