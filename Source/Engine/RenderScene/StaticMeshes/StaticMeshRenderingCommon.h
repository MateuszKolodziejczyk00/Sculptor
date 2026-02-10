#pragma once

#include "SculptorCoreTypes.h"
#include "RGDescriptorSetState.h"
#include "DescriptorSetBindings/RWBufferBinding.h"
#include "DescriptorSetBindings/ConstantBufferBinding.h"
#include "DescriptorSetBindings/SRVTextureBinding.h"
#include "ShaderStructs/ShaderStructs.h"
#include "RGNodeParametersStruct.h"
#include "Types/Buffer.h"
#include "Pipelines/PipelineState.h"
#include "View/SceneView.h"
#include "Geometry/GeometryTypes.h"
#include "Materials/MaterialsRenderingCommon.h"


namespace spt::rg
{
class RenderGraphBuilder;
} // spt::rg


namespace spt::rsc
{

class RenderScene;
class ViewRenderingSpec;
struct StaticMeshSMBatchDefinition;
struct RenderStageExecutionContext;
class RenderMesh;


BEGIN_SHADER_STRUCT(SMIndirectDrawCallData)
	SHADER_STRUCT_FIELD(Uint32, vertexCount)
	SHADER_STRUCT_FIELD(Uint32, instanceCount)
	SHADER_STRUCT_FIELD(Uint32, firstVertex)
	SHADER_STRUCT_FIELD(Uint32, firstInstance)
END_SHADER_STRUCT();


BEGIN_SHADER_STRUCT(SMGPUWorkloadID)
	SHADER_STRUCT_FIELD(Uint32, data1)
END_SHADER_STRUCT();


BEGIN_SHADER_STRUCT(SMDepthOnlyDrawCallData)
	/* Vulkan command data */
	SHADER_STRUCT_FIELD(Uint32, vertexCount)
	SHADER_STRUCT_FIELD(Uint32, instanceCount)
	SHADER_STRUCT_FIELD(Uint32, firstVertex)
	SHADER_STRUCT_FIELD(Uint32, firstInstance)
	
	/* Custom Data */
	SHADER_STRUCT_FIELD(Uint32, batchElementIdx)
	SHADER_STRUCT_FIELD(Uint32, padding0)
	SHADER_STRUCT_FIELD(Uint32, padding1)
	SHADER_STRUCT_FIELD(Uint32, padding2)
END_SHADER_STRUCT();


DS_BEGIN(SMDepthOnlyDrawInstancesDS, rg::RGDescriptorSetState<SMDepthOnlyDrawInstancesDS>)
	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<SMDepthOnlyDrawCallData>),	u_drawCommands)
DS_END();


struct StaticMeshInstanceRenderData
{
	lib::MTHandle<RenderMesh> staticMesh;
};
SPT_REGISTER_COMPONENT_TYPE(StaticMeshInstanceRenderData, RenderSceneRegistry);


BEGIN_SHADER_STRUCT(StaticMeshBatchElement)
	SHADER_STRUCT_FIELD(RenderEntityGPUPtr,      entityPtr)
	SHADER_STRUCT_FIELD(SubmeshGPUPtr,           submeshPtr)
	SHADER_STRUCT_FIELD(mat::MaterialDataHandle, materialDataHandle)
END_SHADER_STRUCT();


BEGIN_SHADER_STRUCT(SMGPUBatchData)
	SHADER_STRUCT_FIELD(Uint32, elementsNum)
END_SHADER_STRUCT();


DS_BEGIN(StaticMeshBatchDS, rg::RGDescriptorSetState<StaticMeshBatchDS>)
	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<StaticMeshBatchElement>), u_batchElements)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<SMGPUBatchData>),           u_batchData)
DS_END();


BEGIN_SHADER_STRUCT(SMDepthOnlyPermutation)
	SHADER_STRUCT_FIELD(mat::MaterialShader, SHADER)
	SHADER_STRUCT_FIELD(Bool,                CUSTOM_OPACITY)
END_SHADER_STRUCT();


struct StaticMeshSMBatchDefinition
{
	StaticMeshSMBatchDefinition()
		: batchElementsNum(0)
		, maxMeshletsNum(0)
		, maxTrianglesNum(0)
	{ }

	Uint32 batchElementsNum;
	Uint32 maxMeshletsNum;
	Uint32 maxTrianglesNum;

	SMDepthOnlyPermutation permutation;

	lib::MTHandle<StaticMeshBatchDS> batchDS;
};

} // spt::rsc
