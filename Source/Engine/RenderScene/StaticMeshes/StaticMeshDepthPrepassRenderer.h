#pragma once

#include "StaticMeshRenderingCommon.h"


namespace spt::rsc
{

DS_BEGIN(SMDepthPrepassCullInstancesDS, rg::RGDescriptorSetState<SMDepthPrepassCullInstancesDS>)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<SMDepthOnlyDrawCallData>),	u_drawCommands)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<Uint32>),					u_drawsCount)
	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<Uint32>),						u_validBatchElementsNum)
DS_END();


BEGIN_RG_NODE_PARAMETERS_STRUCT(SMIndirectDepthPrepassCommandsParameters)
	RG_BUFFER_VIEW(batchDrawCommandsBuffer, rg::ERGBufferAccess::Read, rhi::EPipelineStage::DrawIndirect)
	RG_BUFFER_VIEW(batchDrawCommandsCountBuffer, rg::ERGBufferAccess::Read, rhi::EPipelineStage::DrawIndirect)
END_RG_NODE_PARAMETERS_STRUCT();


struct SMDepthPrepassBatch
{
	lib::SharedPtr<StaticMeshBatchDS>				batchDS;
	
	lib::SharedPtr<SMDepthPrepassCullInstancesDS>	cullInstancesDS;
	lib::SharedPtr<SMDepthOnlyDrawInstancesDS>		drawInstancesDS;

	rg::RGBufferViewHandle drawCommandsBuffer;
	rg::RGBufferViewHandle indirectDrawCountBuffer;

	Uint32 batchedSubmeshesNum;
};


struct SMDepthPrepassBatches
{
	lib::DynamicArray<SMDepthPrepassBatch> batches;
};


class StaticMeshDepthPrepassRenderer
{
public:

	StaticMeshDepthPrepassRenderer();

	Bool BuildBatchesPerView(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec);
	void CullPerView(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec);
	void RenderPerView(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& context);

private:

	SMDepthPrepassBatch CreateBatch(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, const StaticMeshesVisibleLastFrame::BatchElementsInfo& batchElements) const;

	rdr::PipelineStateID m_buildDrawCommandsPipeline;

	rdr::PipelineStateID m_depthPrepassRenderingPipeline;
};

} // spt::rsc