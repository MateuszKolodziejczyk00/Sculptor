#pragma once

#include "StaticMeshRenderingCommon.h"
#include "RenderSceneRegistry.h"


namespace spt::rsc
{

BEGIN_SHADER_STRUCT(SMLightShadowMapsData)
	SHADER_STRUCT_FIELD(Uint32, facesNum)
END_SHADER_STRUCT();


DS_BEGIN(SMShadowMapCullingDS, rg::RGDescriptorSetState<SMShadowMapCullingDS>)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableConstantBufferBinding<SMLightShadowMapsData>),		u_shadowMapsData)
	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<SceneViewCullingData>),				u_viewsCullingData)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<Uint32>),							u_drawsCount)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<SMDepthOnlyDrawCallData>),			u_drawCommandsFace0)
	DS_BINDING(BINDING_TYPE(gfx::OptionalRWStructuredBufferBinding<SMDepthOnlyDrawCallData>),	u_drawCommandsFace1)
	DS_BINDING(BINDING_TYPE(gfx::OptionalRWStructuredBufferBinding<SMDepthOnlyDrawCallData>),	u_drawCommandsFace2)
	DS_BINDING(BINDING_TYPE(gfx::OptionalRWStructuredBufferBinding<SMDepthOnlyDrawCallData>),	u_drawCommandsFace3)
	DS_BINDING(BINDING_TYPE(gfx::OptionalRWStructuredBufferBinding<SMDepthOnlyDrawCallData>),	u_drawCommandsFace4)
	DS_BINDING(BINDING_TYPE(gfx::OptionalRWStructuredBufferBinding<SMDepthOnlyDrawCallData>),	u_drawCommandsFace5)
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
		lib::SharedPtr<SMDepthOnlyDrawInstancesDS>	drawDS;
	};

	lib::SharedPtr<StaticMeshBatchDS> batchDS;

	lib::SharedPtr<SMShadowMapCullingDS> cullingDS;

	lib::DynamicArray<PerFaceData> perFaceData;

	rg::RGBufferViewHandle indirectDrawCountsBuffer;

	Uint32 batchedSubmeshesNum;
};


class StaticMeshShadowMapRenderer
{
public:

	StaticMeshShadowMapRenderer();

	void RenderPerFrame(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene);
	void RenderPerView(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& context);

private:

	SMShadowMapBatch CreateBatch(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, const lib::DynamicArray<RenderView*>& batchedViews, const StaticMeshBatchDefinition& batchDef) const;
	
	void BuildBatchDrawCommands(rg::RenderGraphBuilder& graphBuilder, const SMShadowMapBatch& batch);

	// Right now we need only one batch per light, but if this will change, value of this map should be array of batches
	lib::HashMap<RenderSceneEntity, SMShadowMapBatch> m_pointLightBatches;

	rdr::PipelineStateID m_buildDrawCommandsPipeline;

	rdr::PipelineStateID m_shadowMapRenderingPipeline;
};

} // spt::rsc