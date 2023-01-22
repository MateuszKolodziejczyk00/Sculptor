#pragma once

#include "ShaderStructs/ShaderStructsMacros.h"
#include "Types/Buffer.h"
#include "RGDescriptorSetState.h"
#include "Types/DescriptorSetState/DescriptorSetState.h"
#include "DescriptorSetBindings/RWBufferBinding.h"

namespace spt::rsc
{

struct StaticMeshGeometryData
{
	rhi::RHISuballocation staticMeshSuballocation;
	rhi::RHISuballocation submeshesSuballocation;
	rhi::RHISuballocation meshletsSuballocation;
	rhi::RHISuballocation geometrySuballocation;
};


struct StaticMeshRenderingDefinition
{
	StaticMeshRenderingDefinition()
		: staticMeshIdx(0)
		, maxSubmeshesNum(0)
		, maxMeshletsNum(0)
		, maxTrianglesNum(0)
	{ }

	Uint32 staticMeshIdx;
	// Submeshes of first LOD
	Uint32 maxSubmeshesNum;
	// Meshlets of first LOD
	Uint32 maxMeshletsNum;
	// Triangles of first LOD
	Uint32 maxTrianglesNum;
};


BEGIN_SHADER_STRUCT(, StaticMeshGPUData)
	SHADER_STRUCT_FIELD(Uint32, geometryDataOffset)
	SHADER_STRUCT_FIELD(Uint32, submeshesBeginIdx)
	SHADER_STRUCT_FIELD(Uint32, submeshesNum)
	/* 1 empty byte */
	SHADER_STRUCT_FIELD(math::Vector3f, boundingSphereCenter)
	SHADER_STRUCT_FIELD(Real32, boundingSphereRadius)
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
	/* 2 empty bytes */
	SHADER_STRUCT_FIELD(math::Vector3f, boundingSphereCenter)
	SHADER_STRUCT_FIELD(Real32, boundingSphereRadius)
END_SHADER_STRUCT();


BEGIN_SHADER_STRUCT(, MeshletGPUData)
	SHADER_STRUCT_FIELD(Uint32, triangleCount)
	SHADER_STRUCT_FIELD(Uint32, meshletPrimitivesOffset)
	SHADER_STRUCT_FIELD(Uint32, meshletVerticesOffset)
	SHADER_STRUCT_FIELD(Uint32, packedConeAxisAndCutoff) /* {[uint8 cone cutoff][3 x uint8 cone axis]} */
	SHADER_STRUCT_FIELD(math::Vector3f, boundingSphereCenter)
	SHADER_STRUCT_FIELD(Real32, boundingSphereRadius)
END_SHADER_STRUCT();


DS_BEGIN(StaticMeshUnifiedDataDS, rg::RGDescriptorSetState<StaticMeshUnifiedDataDS>)
	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<StaticMeshGPUData>),	u_staticMeshes)
	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<SubmeshGPUData>),		u_submeshes)
	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<MeshletGPUData>),		u_meshlets)
DS_END();


struct StaticMeshBuildData
{
	math::Vector3f boundingSphereCenter;
	Real32 boundingSphereRadius;
};


class StaticMeshUnifiedData
{
public:

	static StaticMeshUnifiedData& Get();

	StaticMeshGeometryData BuildStaticMeshData(const StaticMeshBuildData& buildData, lib::DynamicArray<SubmeshGPUData>& submeshes, lib::DynamicArray<MeshletGPUData>& meshlets, rhi::RHISuballocation geometryDataSuballocation);

	const lib::SharedPtr<StaticMeshUnifiedDataDS>& GetUnifiedDataDS() const;

private:

	StaticMeshUnifiedData();
	void DestroyResources();

	lib::SharedPtr<rdr::Buffer> m_staticMeshesBuffer;
	lib::SharedPtr<rdr::Buffer> m_submeshesBuffer;
	lib::SharedPtr<rdr::Buffer> m_meshletsBuffer;

	lib::SharedPtr<StaticMeshUnifiedDataDS> m_unifiedDataDS;
};

} // spt::rsc
