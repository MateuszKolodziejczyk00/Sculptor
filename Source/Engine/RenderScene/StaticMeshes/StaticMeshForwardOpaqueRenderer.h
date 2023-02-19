#pragma once

#include "StaticMeshRenderingCommon.h"


namespace spt::rsc
{

DS_BEGIN(SMCullSubmeshesDS, rg::RGDescriptorSetState<SMCullSubmeshesDS>)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<StaticMeshBatchElement>),	u_visibleBatchElements)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<Uint32>),					u_dispatchVisibleBatchElemsParams)
DS_END();


DS_BEGIN(SMCullMeshletsDS, rg::RGDescriptorSetState<SMCullMeshletsDS>)
	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<StaticMeshBatchElement>),	u_visibleBatchElements)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<SMGPUWorkloadID>),		u_meshletWorkloads)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<Uint32>),				u_dispatchMeshletsParams)
DS_END();


DS_BEGIN(SMCullTrianglesDS, rg::RGDescriptorSetState<SMCullTrianglesDS>)
	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<StaticMeshBatchElement>),		u_visibleBatchElements)
	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<SMGPUWorkloadID>),				u_meshletWorkloads)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<SMGPUWorkloadID>),			u_triangleWorkloads)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<SMIndirectDrawCallData>),	u_drawTrianglesParams)
DS_END();


DS_BEGIN(SMIndirectRenderTrianglesDS, rg::RGDescriptorSetState<SMIndirectRenderTrianglesDS>)
	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<StaticMeshBatchElement>),	u_visibleBatchElements)
	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<SMGPUWorkloadID>),			u_triangleWorkloads)
DS_END();


BEGIN_RG_NODE_PARAMETERS_STRUCT(SMIndirectTrianglesBatchDrawParams)
	RG_BUFFER_VIEW(batchDrawCommandsBuffer, rg::ERGBufferAccess::Read, rhi::EPipelineStage::DrawIndirect)
END_RG_NODE_PARAMETERS_STRUCT();


struct SMForwardOpaqueBatch
{
	lib::SharedPtr<rdr::Buffer> visibleBatchElementsBufferInstance;
	lib::SharedPtr<rdr::Buffer> dispatchVisibleBatchElemsParamsBufferInstance;

	rg::RGBufferViewHandle visibleBatchElementsDispatchParamsBuffer;
	rg::RGBufferViewHandle meshletsWorkloadsDispatchArgsBuffer;
	rg::RGBufferViewHandle drawTrianglesBatchArgsBuffer;
	
	lib::SharedPtr<StaticMeshBatchDS>			batchDS;
	
	lib::SharedPtr<SMCullSubmeshesDS>			cullSubmeshesDS;
	lib::SharedPtr<SMCullMeshletsDS>			cullMeshletsDS;
	lib::SharedPtr<SMCullTrianglesDS>			cullTrianglesDS;
	lib::SharedPtr<SMIndirectRenderTrianglesDS>	indirectRenderTrianglesDS;

	Uint32 batchedSubmeshesNum;
};


struct SMOpaqueForwardBatches
{
	lib::DynamicArray<SMForwardOpaqueBatch> batches;
};


class StaticMeshForwardOpaqueRenderer
{
public:

	StaticMeshForwardOpaqueRenderer();

	Bool BuildBatchesPerView(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec);
	void CullPerView(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& context);
	void RenderPerView(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& context);

private:

	SMForwardOpaqueBatch CreateBatch(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, const StaticMeshBatchDefinition& batchDef);

	rdr::PipelineStateID m_cullSubmeshesPipeline;
	rdr::PipelineStateID m_cullMeshletsPipeline;
	rdr::PipelineStateID m_cullTrianglesPipeline;

	rdr::PipelineStateID m_forwadOpaqueShadingPipeline;
};

} // spt::rsc