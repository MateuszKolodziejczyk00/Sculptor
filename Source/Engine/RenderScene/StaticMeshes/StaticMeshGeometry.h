#pragma once

#include "ShaderStructs/ShaderStructs.h"
#include "Types/Buffer.h"
#include "RGDescriptorSetState.h"
#include "Types/DescriptorSetState/DescriptorSetState.h"
#include "DescriptorSetBindings/RWBufferBinding.h"
#include "ECSRegistry.h"
#include "ComponentsRegistry.h"
#include "Bindless/BindlessTypes.h"
#include "Bindless/NamedBuffers.h"


namespace spt::rsc
{

struct StaticMeshGeometryData
{
	rhi::RHIVirtualAllocation submeshesSuballocation;
	rhi::RHIVirtualAllocation meshletsSuballocation;
	rhi::RHIVirtualAllocation geometrySuballocation;
};


struct SubmeshRenderingDefinition
{
	Uint32 trianglesNum = 0u;
	Uint32 verticesNum  = 0u;
	Uint32 meshletsNum  = 0u;

	rhi::DeviceAddress locationsAddress = 0u;
	rhi::DeviceAddress indicesAddress   = 0u;
};


BEGIN_SHADER_STRUCT(MeshletGPUData)
	SHADER_STRUCT_FIELD(Uint16, triangleCount)
	SHADER_STRUCT_FIELD(Uint16, vertexCount)
	SHADER_STRUCT_FIELD(Uint32, meshletPrimitivesOffset)
	SHADER_STRUCT_FIELD(Uint32, meshletVerticesOffset)
	SHADER_STRUCT_FIELD(Uint32, packedConeAxisAndCutoff) /* {[uint8 cone cutoff][3 x uint8 cone axis]} */
	SHADER_STRUCT_FIELD(math::Vector3f, boundingSphereCenter)
	SHADER_STRUCT_FIELD(Real32, boundingSphereRadius)
END_SHADER_STRUCT();


CREATE_NAMED_BUFFER(MeshletsArray, MeshletGPUData);
using MeshletGPUPtr = gfx::GPUNamedElemPtr<MeshletsArray, MeshletGPUData>;
using MeshletsGPUSpan = gfx::GPUNamedElemsSpan<MeshletsArray, MeshletGPUData>;


BEGIN_SHADER_STRUCT(SubmeshGPUData)
	SHADER_STRUCT_FIELD(Uint32,          indicesOffset)
	SHADER_STRUCT_FIELD(Uint32,          indicesNum)
	SHADER_STRUCT_FIELD(Uint32,          locationsOffset)
	SHADER_STRUCT_FIELD(Uint32,          normalsOffset)
	SHADER_STRUCT_FIELD(Uint32,          tangentsOffset)
	SHADER_STRUCT_FIELD(Uint32,          uvsOffset)
	SHADER_STRUCT_FIELD(Uint32,          meshletsPrimitivesDataOffset)
	SHADER_STRUCT_FIELD(Uint32,          meshletsVerticesDataOffset)
	SHADER_STRUCT_FIELD(MeshletsGPUSpan, meshlets)
	SHADER_STRUCT_FIELD(math::Vector3f,  boundingSphereCenter)
	SHADER_STRUCT_FIELD(Real32,          boundingSphereRadius)
END_SHADER_STRUCT();


CREATE_NAMED_BUFFER(SubmeshesArray, SubmeshGPUData);
using SubmeshGPUPtr   = gfx::GPUNamedElemPtr<SubmeshesArray, SubmeshGPUData>;
using SubmeshsGPUSpan = gfx::GPUNamedElemsSpan<SubmeshesArray, SubmeshGPUData>;


BEGIN_SHADER_STRUCT(StaticMeshGeometryBuffers)
	SHADER_STRUCT_FIELD(SubmeshesArray, submeshesArray)
	SHADER_STRUCT_FIELD(MeshletsArray,  meshletsArray)
END_SHADER_STRUCT();


class StaticMeshUnifiedData
{
public:

	static StaticMeshUnifiedData& Get();

	StaticMeshGeometryData BuildStaticMeshData(lib::DynamicArray<SubmeshGPUData>& submeshes, lib::DynamicArray<MeshletGPUData>& meshlets, rhi::RHIVirtualAllocation geometryDataSuballocation);

	StaticMeshGeometryBuffers GetGeometryBuffers() const;

private:

	StaticMeshUnifiedData();
	void DestroyResources();

	lib::SharedPtr<rdr::Buffer> m_submeshesBuffer;
	lib::SharedPtr<rdr::Buffer> m_meshletsBuffer;
};

} // spt::rsc
