#pragma once

#include "StaticMeshRenderingCommon.h"
#include "RGNodeParametersStruct.h"
#include "View/ViewRenderingSpec.h"


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
	rg::RGBufferViewHandle visibleBatchElementsDispatchParamsBuffer;
	rg::RGBufferViewHandle meshletsWorkloadsDispatchArgsBuffer;
	rg::RGBufferViewHandle drawTrianglesBatchArgsBuffer;
	
	lib::MTHandle<StaticMeshBatchDS> batchDS;
	
	lib::MTHandle<SMCullSubmeshesDS>           cullSubmeshesDS;
	lib::MTHandle<SMCullMeshletsDS>            cullMeshletsDS;
	lib::MTHandle<SMCullTrianglesDS>           cullTrianglesDS;
	lib::MTHandle<SMIndirectRenderTrianglesDS> indirectRenderTrianglesDS;

	Uint32 batchedSubmeshesNum;

	mat::MaterialShadersHash materialShadersHash;
};


struct SMOpaqueForwardBatches
{
	lib::DynamicArray<SMForwardOpaqueBatch> batches;
};


class RENDER_SCENE_API StaticMeshForwardOpaqueRenderer
{
public:

	StaticMeshForwardOpaqueRenderer();

	Bool BuildBatchesPerView(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const lib::DynamicArray<StaticMeshBatchDefinition>& batchDefinitions);
	void CullPerView(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& context);
	void RenderPerView(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& context, RenderStageContextMetaDataHandle metaData);

private:

	SMForwardOpaqueBatch CreateBatch(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, const StaticMeshBatchDefinition& batchDef) const;

	rdr::PipelineStateID GetShadingPipeline(const SMForwardOpaqueBatch& batch, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const SceneRendererSettings& rendererSettings) const;

	rdr::PipelineStateID m_cullSubmeshesPipeline;
	rdr::PipelineStateID m_cullMeshletsPipeline;
	rdr::PipelineStateID m_cullTrianglesPipeline;
};

} // spt::rsc