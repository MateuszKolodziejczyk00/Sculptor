#pragma once

#include "StaticMeshRenderingCommon.h"
#include "RenderSceneRegistry.h"
#include "View/ViewRenderingSpec.h"


namespace spt::rsc
{

BEGIN_SHADER_STRUCT(SMLightShadowMapsData)
	SHADER_STRUCT_FIELD(Uint32, facesNum)
END_SHADER_STRUCT();


DS_BEGIN(SMShadowMapCullingDS, rg::RGDescriptorSetState<SMShadowMapCullingDS>)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<SMLightShadowMapsData>),               u_shadowMapsData)
	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<SceneViewCullingData>),              u_viewsCullingData)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<Uint32>),                          u_drawsCount)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<SMDepthOnlyDrawCallData>),         u_drawCommandsFace0)
	DS_BINDING(BINDING_TYPE(gfx::OptionalRWStructuredBufferBinding<SMDepthOnlyDrawCallData>), u_drawCommandsFace1)
	DS_BINDING(BINDING_TYPE(gfx::OptionalRWStructuredBufferBinding<SMDepthOnlyDrawCallData>), u_drawCommandsFace2)
	DS_BINDING(BINDING_TYPE(gfx::OptionalRWStructuredBufferBinding<SMDepthOnlyDrawCallData>), u_drawCommandsFace3)
	DS_BINDING(BINDING_TYPE(gfx::OptionalRWStructuredBufferBinding<SMDepthOnlyDrawCallData>), u_drawCommandsFace4)
	DS_BINDING(BINDING_TYPE(gfx::OptionalRWStructuredBufferBinding<SMDepthOnlyDrawCallData>), u_drawCommandsFace5)
DS_END();


BEGIN_RG_NODE_PARAMETERS_STRUCT(SMIndirectShadowMapCommandsParameters)
	RG_BUFFER_VIEW(batchDrawCommandsBuffer, rg::ERGBufferAccess::Read, rhi::EPipelineStage::DrawIndirect)
	RG_BUFFER_VIEW(batchDrawCommandsCountBuffer, rg::ERGBufferAccess::Read, rhi::EPipelineStage::DrawIndirect)
END_RG_NODE_PARAMETERS_STRUCT();


struct SMShadowMapBatch
{
	SMShadowMapBatch()
		: batchedSubmeshesNum(0)
	{ }

	struct PerFaceData
	{
		rg::RGBufferViewHandle						drawCommandsBuffer;
		lib::MTHandle<SMDepthOnlyDrawInstancesDS>	drawDS;
	};

	lib::MTHandle<StaticMeshBatchDS> batchDS;

	lib::MTHandle<SMShadowMapCullingDS> cullingDS;

	lib::DynamicArray<PerFaceData> perFaceData;

	rg::RGBufferViewHandle indirectDrawCountsBuffer;

	Uint32 batchedSubmeshesNum;

	SMDepthOnlyPermutation permutation;
};


class RENDER_SCENE_API StaticMeshShadowMapRenderer
{
public:

	StaticMeshShadowMapRenderer();

	void RenderPerFrame(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, const lib::DynamicArray<ViewRenderingSpec*>& viewSpecs);

	void RenderToShadowMap(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& context, RenderStageContextMetaDataHandle metaData);

	void FinishRenderingFrame();

private:

	SMShadowMapBatch CreateBatch(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, const lib::DynamicArray<RenderView*>& batchedViews, const StaticMeshSMBatchDefinition& batchDef) const;
	
	void BuildBatchDrawCommands(rg::RenderGraphBuilder& graphBuilder, const SMShadowMapBatch& batch);

	lib::HashMap<RenderSceneEntity, lib::DynamicArray<SMShadowMapBatch>> m_pointLightBatches;
	lib::HashMap<const RenderView*, lib::DynamicArray<SMShadowMapBatch>> m_globalShadowViewBatches;

	rdr::PipelineStateID m_buildDrawCommandsPipeline;
};

} // spt::rsc