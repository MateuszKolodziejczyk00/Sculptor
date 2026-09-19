#pragma once

#include "SculptorCoreTypes.h"
#include "ShaderStructs/ShaderStructs.h"
#include "Material.h"
#include "RenderSceneTypes.h"
#include "StaticMeshes/StaticMeshGeometry.h"


namespace spt::rsc
{

BEGIN_SHADER_STRUCT(GeometryBatchElement)
	SHADER_STRUCT_FIELD(RenderEntityGPUPtr,      entityPtr)
	SHADER_STRUCT_FIELD(SubmeshGPUPtr,           submeshPtr)
	SHADER_STRUCT_FIELD(mat::MaterialDataHandle, materialDataHandle)
	SHADER_STRUCT_FIELD(Uint16,                  materialBatchIdx)
END_SHADER_STRUCT();


BEGIN_SHADER_STRUCT(GPUVisibleMeshlet)
	SHADER_STRUCT_FIELD(RenderEntityGPUPtr,      entityPtr)
	SHADER_STRUCT_FIELD(SubmeshGPUPtr,           submeshPtr)
	SHADER_STRUCT_FIELD(MeshletGPUPtr,           meshletPtr)
	SHADER_STRUCT_FIELD(mat::MaterialDataHandle, materialDataHandle)
	SHADER_STRUCT_FIELD(Uint16,                  materialBatchIdx)
END_SHADER_STRUCT();


BEGIN_SHADER_STRUCT(GeometryGPUBatchData)
	SHADER_STRUCT_FIELD(Uint32,                                    elementsNum)
	SHADER_STRUCT_FIELD(gfx::TypedBufferRef<GeometryBatchElement>, batchElements)
END_SHADER_STRUCT();


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
	SHADER_STRUCT_FIELD(Bool,                MATERIAL_ENABLE_POM)
END_SHADER_STRUCT();


struct GeometryBatch
{
	Uint32 batchElementsNum = 0u;
	Uint32 batchMeshletsNum = 0u;
	GeometryBatchPermutation permutation;
	GeometryGPUBatchData batchData;
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
