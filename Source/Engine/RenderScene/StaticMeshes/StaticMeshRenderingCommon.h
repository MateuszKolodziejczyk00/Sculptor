#pragma once

#include "SculptorCoreTypes.h"
#include "RGDescriptorSetState.h"
#include "DescriptorSetBindings/RWBufferBinding.h"
#include "DescriptorSetBindings/ConstantBufferBinding.h"
#include "DescriptorSetBindings/SRVTextureBinding.h"
#include "ShaderStructs/ShaderStructsMacros.h"
#include "RGNodeParametersStruct.h"
#include "Types/Buffer.h"
#include "Pipelines/PipelineState.h"
#include "View/SceneView.h"
#include "StaticMeshPrimitivesSystem.h"


namespace spt::rg
{
class RenderGraphBuilder;
} // spt::rg


namespace spt::rsc
{

class RenderScene;
class ViewRenderingSpec;
struct StaticMeshBatchDefinition;
struct RenderStageExecutionContext;


BEGIN_SHADER_STRUCT(SMIndirectDrawCallData)
	SHADER_STRUCT_FIELD(Uint32, vertexCount)
	SHADER_STRUCT_FIELD(Uint32, instanceCount)
	SHADER_STRUCT_FIELD(Uint32, firstVertex)
	SHADER_STRUCT_FIELD(Uint32, firstInstance)
END_SHADER_STRUCT();


BEGIN_SHADER_STRUCT(SMGPUWorkloadID)
	SHADER_STRUCT_FIELD(Uint32, data1)
END_SHADER_STRUCT();


BEGIN_SHADER_STRUCT(SMGPUBatchData)
	SHADER_STRUCT_FIELD(Uint32, elementsNum)
END_SHADER_STRUCT();


BEGIN_SHADER_STRUCT(SMViewRenderingParameters)
	SHADER_STRUCT_FIELD(math::Vector2u, rtResolution)
END_SHADER_STRUCT();


DS_BEGIN(SMProcessBatchForViewDS, rg::RGDescriptorSetState<SMProcessBatchForViewDS>)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<SceneViewData>),				u_sceneView)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<SceneViewCullingData>),		u_cullingData)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<SMViewRenderingParameters>),	u_viewRenderingParams)
DS_END();


DS_BEGIN(StaticMeshBatchDS, rg::RGDescriptorSetState<StaticMeshBatchDS>)
	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<StaticMeshBatchElement>),	u_batchElements)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<SMGPUBatchData>),			u_batchData)
DS_END();


struct SMRenderingViewData
{
	lib::SharedPtr<SMProcessBatchForViewDS> viewDS;
};


struct StaticMeshesVisibleLastFrame
{
	struct BatchElementsInfo
	{
		lib::SharedPtr<rdr::Buffer> visibleBatchElementsBuffer;
		lib::SharedPtr<rdr::Buffer> batchElementsDispatchParamsBuffer;

		Uint32 batchedSubmeshesNum;
	};

	lib::DynamicArray<BatchElementsInfo> visibleInstances;
};

} // spt::rsc
