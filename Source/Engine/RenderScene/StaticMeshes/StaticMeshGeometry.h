#pragma once

#include "ShaderStructs/ShaderStructs.h"
#include "Types/Buffer.h"
#include "RGDescriptorSetState.h"
#include "Types/DescriptorSetState/DescriptorSetState.h"
#include "DescriptorSetBindings/RWBufferBinding.h"
#include "ECSRegistry.h"
#include "ComponentsRegistry.h"


namespace spt::rsc
{

struct StaticMeshGeometryData
{
	rhi::RHIVirtualAllocation submeshesSuballocation;
	rhi::RHIVirtualAllocation meshletsSuballocation;
	rhi::RHIVirtualAllocation geometrySuballocation;
};
SPT_REGISTER_COMPONENT_TYPE(StaticMeshGeometryData, ecs::Registry);


struct SubmeshRenderingDefinition
{
	Uint32 trianglesNum;
	Uint32 meshletsNum;
};


struct StaticMeshRenderingDefinition
{
	StaticMeshRenderingDefinition()
		: geometryDataOffset(0)
		, submeshesBeginIdx(0)
		, boundingSphereCenter(math::Vector3f::Zero())
		, boundingSphereRadius(0.f)
	{ }
	
	Uint32 geometryDataOffset;
	Uint32 submeshesBeginIdx;
	
	math::Vector3f boundingSphereCenter;
	Real32 boundingSphereRadius;

	lib::DynamicArray<SubmeshRenderingDefinition> submeshesDefs;
};
SPT_REGISTER_COMPONENT_TYPE(StaticMeshRenderingDefinition, ecs::Registry);

BEGIN_SHADER_STRUCT(SubmeshGPUData)
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
	/* 8 empty bytes */
	SHADER_STRUCT_FIELD(math::Vector3f, boundingSphereCenter)
	SHADER_STRUCT_FIELD(Real32, boundingSphereRadius)
END_SHADER_STRUCT();


BEGIN_SHADER_STRUCT(MeshletGPUData)
	SHADER_STRUCT_FIELD(Uint16, triangleCount)
	SHADER_STRUCT_FIELD(Uint16, vertexCount)
	SHADER_STRUCT_FIELD(Uint32, meshletPrimitivesOffset)
	SHADER_STRUCT_FIELD(Uint32, meshletVerticesOffset)
	SHADER_STRUCT_FIELD(Uint32, packedConeAxisAndCutoff) /* {[uint8 cone cutoff][3 x uint8 cone axis]} */
	SHADER_STRUCT_FIELD(math::Vector3f, boundingSphereCenter)
	SHADER_STRUCT_FIELD(Real32, boundingSphereRadius)
END_SHADER_STRUCT();


DS_BEGIN(StaticMeshUnifiedDataDS, rg::RGDescriptorSetState<StaticMeshUnifiedDataDS>)
	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<SubmeshGPUData>),		u_submeshes)
	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<MeshletGPUData>),		u_meshlets)
DS_END();


class StaticMeshUnifiedData
{
public:

	static StaticMeshUnifiedData& Get();

	StaticMeshGeometryData BuildStaticMeshData(lib::DynamicArray<SubmeshGPUData>& submeshes, lib::DynamicArray<MeshletGPUData>& meshlets, rhi::RHIVirtualAllocation geometryDataSuballocation);

	const lib::MTHandle<StaticMeshUnifiedDataDS>& GetUnifiedDataDS() const;

private:

	StaticMeshUnifiedData();
	void DestroyResources();

	lib::SharedPtr<rdr::Buffer> m_submeshesBuffer;
	lib::SharedPtr<rdr::Buffer> m_meshletsBuffer;

	lib::MTHandle<StaticMeshUnifiedDataDS> m_unifiedDataDS;
};

} // spt::rsc
