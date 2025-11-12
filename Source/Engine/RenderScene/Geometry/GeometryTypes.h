#pragma once

#include "SculptorCoreTypes.h"
#include "ShaderStructs/ShaderStructs.h"
#include "Material.h"
#include "RGDescriptorSetState.h"
#include "DescriptorSetBindings/RWBufferBinding.h"
#include "DescriptorSetBindings/ConstantBufferBinding.h"
#include "RenderSceneTypes.h"
#include "StaticMeshes/StaticMeshGeometry.h"


namespace spt::rsc
{

BEGIN_SHADER_STRUCT(GeometryBatchElement)
	SHADER_STRUCT_FIELD(RenderEntityGPUPtr, entityPtr)
	SHADER_STRUCT_FIELD(SubmeshGPUPtr,      submeshPtr)
	SHADER_STRUCT_FIELD(Uint16,             materialDataID)
	SHADER_STRUCT_FIELD(Uint16,             materialBatchIdx)
END_SHADER_STRUCT();


BEGIN_SHADER_STRUCT(GPUVisibleMeshlet)
	SHADER_STRUCT_FIELD(RenderEntityGPUPtr, entityPtr)
	SHADER_STRUCT_FIELD(SubmeshGPUPtr,      submeshPtr)
	SHADER_STRUCT_FIELD(MeshletGPUPtr,      meshletPtr)
	SHADER_STRUCT_FIELD(Uint16,             materialDataID)
	SHADER_STRUCT_FIELD(Uint16,             materialBatchIdx)
END_SHADER_STRUCT();


BEGIN_SHADER_STRUCT(GeometryGPUBatchData)
	SHADER_STRUCT_FIELD(Uint32, elementsNum)
END_SHADER_STRUCT();


DS_BEGIN(GeometryBatchDS, rg::RGDescriptorSetState<GeometryBatchDS>)
	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<GeometryBatchElement>), u_batchElements)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<GeometryGPUBatchData>),   u_batchData)
DS_END();


// Batch for visiblity buffer generation
BEGIN_SHADER_STRUCT(GeometryBatchPermutation)
	SHADER_STRUCT_FIELD(mat::MaterialShader, SHADER)
	SHADER_STRUCT_FIELD(Bool,                DOUBLE_SIDED)
	SHADER_STRUCT_FIELD(Bool,                CUSTOM_OPACITY)
END_SHADER_STRUCT();


// Batch for g-buffer buffer generation
BEGIN_SHADER_STRUCT(MaterialBatchPermutation)
	SHADER_STRUCT_FIELD(mat::MaterialShader, SHADER)
	SHADER_STRUCT_FIELD(Bool,                DOUBLE_SIDED)
END_SHADER_STRUCT();


struct GeometryBatch
{
	Uint32 batchElementsNum = 0u;
	Uint32 batchMeshletsNum = 0u;
	GeometryBatchPermutation permutation;

	lib::MTHandle<GeometryBatchDS> batchDS;
};


struct MaterialBatch
{
	MaterialBatchPermutation permutation;
};


struct GeometryPassDataCollection
{
	lib::DynamicArray<GeometryBatch> geometryBatches;
	lib::DynamicArray<MaterialBatch> materialBatches;
};

} // spt::rsc
