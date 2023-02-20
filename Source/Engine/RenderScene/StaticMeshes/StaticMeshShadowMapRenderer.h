#pragma once

#include "StaticMeshRenderingCommon.h"
#include "RenderSceneRegistry.h"


namespace spt::rsc
{

BEGIN_SHADER_STRUCT(SMShadowMapDataPerView)
	SHADER_STRUCT_FIELD(Uint32, faceIdx)
END_SHADER_STRUCT();


DS_BEGIN(SMShadowMapCullingDS, rg::RGDescriptorSetState<SMShadowMapCullingDS>)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<SceneViewCullingData>),			u_cullingData)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<SMShadowMapDataPerView>),		u_shadowMapViewData)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<SMDepthOnlyDrawCallData>),	u_drawCommands)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<Uint32>),					u_drawsCount)
DS_END();


BEGIN_RG_NODE_PARAMETERS_STRUCT(SMIndirectShadowMapCommandsParameters)
	RG_BUFFER_VIEW(batchDrawCommandsBuffer, rg::ERGBufferAccess::Read, rhi::EPipelineStage::DrawIndirect)
	RG_BUFFER_VIEW(batchDrawCommandsCountBuffer, rg::ERGBufferAccess::Read, rhi::EPipelineStage::DrawIndirect)
END_RG_NODE_PARAMETERS_STRUCT();


struct SMShadowMapBatch
{
	struct PerFaceData
	{
		rg::RGBufferViewHandle						drawCommandsBuffers;
		lib::SharedPtr<SMShadowMapCullingDS>		cullingDS;
		lib::SharedPtr<SMDepthOnlyDrawInstancesDS>	drawDS;
	};

	lib::SharedPtr<StaticMeshBatchDS> batchDS;

	lib::DynamicArray<PerFaceData> perFaceData;

	rg::RGBufferViewHandle indirectDrawCountsBuffer;

};

class StaticMeshShadowMapRenderer
{
public:

	StaticMeshShadowMapRenderer();

	void RenderPerFrame(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene);

private:

	SMShadowMapBatch CreateBatch(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, const lib::DynamicArray<RenderView*>& batchedViews, const StaticMeshBatchDefinition& batchDef) const;

	lib::HashMap<RenderSceneEntity, SMShadowMapBatch> m_pointLightBatches;
};

} // spt::rsc