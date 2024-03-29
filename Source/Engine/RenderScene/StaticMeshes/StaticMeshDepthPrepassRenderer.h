#pragma once

#include "StaticMeshRenderingCommon.h"
#include "View/ViewRenderingSpec.h"


namespace spt::rsc
{

struct DepthPrepassMetaData;


DS_BEGIN(SMDepthPrepassCullInstancesDS, rg::RGDescriptorSetState<SMDepthPrepassCullInstancesDS>)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<SMDepthOnlyDrawCallData>),	u_drawCommands)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<Uint32>),					u_drawsCount)
DS_END();


BEGIN_RG_NODE_PARAMETERS_STRUCT(SMIndirectDepthPrepassCommandsParameters)
	RG_BUFFER_VIEW(batchDrawCommandsBuffer, rg::ERGBufferAccess::Read, rhi::EPipelineStage::DrawIndirect)
	RG_BUFFER_VIEW(batchDrawCommandsCountBuffer, rg::ERGBufferAccess::Read, rhi::EPipelineStage::DrawIndirect)
END_RG_NODE_PARAMETERS_STRUCT();


struct SMDepthPrepassBatch
{
	lib::MTHandle<StaticMeshBatchDS>				batchDS;
	
	lib::MTHandle<SMDepthPrepassCullInstancesDS>	cullInstancesDS;
	lib::MTHandle<SMDepthOnlyDrawInstancesDS>		drawInstancesDS;

	rg::RGBufferViewHandle drawCommandsBuffer;
	rg::RGBufferViewHandle indirectDrawCountBuffer;

	Uint32 batchedSubmeshesNum = 0u;

	mat::MaterialShadersHash materialShadersHash;
};


struct SMDepthPrepassBatches
{
	lib::DynamicArray<SMDepthPrepassBatch> batches;
};


class RENDER_SCENE_API StaticMeshDepthPrepassRenderer
{
public:

	StaticMeshDepthPrepassRenderer();

	Bool BuildBatchesPerView(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const lib::DynamicArray<StaticMeshBatchDefinition>& batchDefinitions);
	void CullPerView(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec);
	void RenderPerView(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& context, RenderStageContextMetaDataHandle metaData);

private:

	SMDepthPrepassBatch CreateBatch(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, const StaticMeshBatchDefinition& batchDef) const;
	
	rdr::PipelineStateID GetPipelineStateForBatch(const SMDepthPrepassBatch& batch, const DepthPrepassMetaData& prepassMetaData) const;

	rdr::PipelineStateID m_buildDrawCommandsPipeline;
};

} // spt::rsc
