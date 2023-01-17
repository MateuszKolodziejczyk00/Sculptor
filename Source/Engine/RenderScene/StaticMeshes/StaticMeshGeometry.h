#pragma once

#include "ShaderStructs/ShaderStructsMacros.h"
#include "Types/Buffer.h"

namespace spt::rsc
{

struct StaticMeshGeometryData
{
	rhi::RHISuballocation staticMeshSuballocation;
	rhi::RHISuballocation submeshesSuballocation;
	rhi::RHISuballocation meshletsSuballocation;
	rhi::RHISuballocation geometrySuballocation;
};


BEGIN_SHADER_STRUCT(, StaticMeshGPUData)
	SHADER_STRUCT_FIELD(Uint32, geometryDataOffset)
	SHADER_STRUCT_FIELD(Uint32, submeshesBeginIdx)
	SHADER_STRUCT_FIELD(Uint32, submeshesNum)
	SHADER_STRUCT_FIELD(Uint32, padding1)
END_SHADER_STRUCT();


BEGIN_SHADER_STRUCT(, SubmeshGPUData)
	SHADER_STRUCT_FIELD(Uint32, indicesOffset)
	SHADER_STRUCT_FIELD(Uint32, indicesNum)
	SHADER_STRUCT_FIELD(Uint32, locationsOffset)
	SHADER_STRUCT_FIELD(Uint32, normalsOffset)
	SHADER_STRUCT_FIELD(Uint32, tangentsOffset)
	SHADER_STRUCT_FIELD(Uint32, uvsOffset)
	SHADER_STRUCT_FIELD(Uint32, meshletsPrimitivesDataOffset)
	SHADER_STRUCT_FIELD(Uint32, meshletsVerticesDataOffset)
	SHADER_STRUCT_FIELD(Uint32, meshletsBeginIdx)
	SHADER_STRUCT_FIELD(Uint32, meshletsNum)
	SHADER_STRUCT_FIELD(Uint32, padding0)
	SHADER_STRUCT_FIELD(Uint32, padding1)
END_SHADER_STRUCT();


BEGIN_SHADER_STRUCT(, MeshletGPUData)
	SHADER_STRUCT_FIELD(Uint32, triangleCount)
	SHADER_STRUCT_FIELD(Uint32, meshletPrimitivesOffset)
	SHADER_STRUCT_FIELD(Uint32, meshletVerticesOffset)
	SHADER_STRUCT_FIELD(Uint32, padding)
END_SHADER_STRUCT();


class StaticMeshUnifiedData
{
public:

	static StaticMeshUnifiedData& Get();

	StaticMeshGeometryData BuildStaticMeshData(lib::DynamicArray<SubmeshGPUData>& submeshes, lib::DynamicArray<MeshletGPUData>& meshlets, rhi::RHISuballocation geometryDataSuballocation);

private:

	StaticMeshUnifiedData();
	void DestroyResources();

	lib::SharedPtr<rdr::Buffer> m_staticMeshesBuffer;
	lib::SharedPtr<rdr::Buffer> m_submeshesBuffer;
	lib::SharedPtr<rdr::Buffer> m_meshletsBuffer;
};

} // spt::rsc
