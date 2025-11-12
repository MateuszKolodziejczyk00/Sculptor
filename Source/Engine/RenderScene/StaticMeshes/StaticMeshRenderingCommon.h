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
#include "StaticMeshRenderSceneSubsystem.h"


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

} // spt::rsc
