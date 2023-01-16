#pragma once

#include "SculptorCoreTypes.h"
#include "ShaderStructs/ShaderStructsMacros.h"
#include "Types/Buffer.h"
#include "RGDescriptorSetState.h"
#include "DescriptorSetBindings/RWBufferBinding.h"


namespace spt::rsc
{

BEGIN_SHADER_STRUCT(, PrimitiveGeometryInfo)
	SHADER_STRUCT_FIELD(Uint32, indicesOffset)
	SHADER_STRUCT_FIELD(Uint32, indicesNum)
	SHADER_STRUCT_FIELD(Uint32, locationsOffset)
	SHADER_STRUCT_FIELD(Uint32, normalsOffset)
	SHADER_STRUCT_FIELD(Uint32, tangentsOffset)
	SHADER_STRUCT_FIELD(Uint32, uvsOffset)
	SHADER_STRUCT_FIELD(Uint32, padding0)
	SHADER_STRUCT_FIELD(Uint32, padding1)
END_SHADER_STRUCT();

BEGIN_SHADER_STRUCT(, SubmeshData)
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


BEGIN_SHADER_STRUCT(, MeshletData)
	SHADER_STRUCT_FIELD(Uint32, triangleCount)
	SHADER_STRUCT_FIELD(Uint32, meshletPrimitivesOffset)
	SHADER_STRUCT_FIELD(Uint32, meshletVerticesOffset)
	SHADER_STRUCT_FIELD(Uint32, padding)
END_SHADER_STRUCT();


DS_BEGIN(, PrimitivesDS, rg::RGDescriptorSetState<PrimitivesDS>, rhi::EShaderStageFlags::Compute)
	DS_BINDING(BINDING_TYPE(gfx::ByteAddressBuffer),	primitivesData)
DS_END();


DS_BEGIN(, GeometryDS, rg::RGDescriptorSetState<GeometryDS>, rhi::EShaderStageFlags::Vertex)
	DS_BINDING(BINDING_TYPE(gfx::ByteAddressBuffer),	primitivesData)
	DS_BINDING(BINDING_TYPE(gfx::ByteAddressBuffer),	geometryData)
DS_END();


class GeometryManager
{
public:

	static GeometryManager& Get();

	rhi::RHISuballocation CreateGeometry(const Byte* geometryData, Uint64 dataSize);
	rhi::RHISuballocation CreateGeometry(Uint64 dataSize);

	rhi::RHISuballocation CreatePrimitiveRenderData(const Byte* primitiveData, Uint64 dataSize);
	rhi::RHISuballocation CreatePrimitiveRenderData(Uint64 dataSize);

	const lib::SharedPtr<PrimitivesDS>&	GetPrimitivesDSState() const;
	const lib::SharedPtr<GeometryDS>&	GetGeometryDSState() const;

private:

	GeometryManager();
	void DestroyResources();

	lib::SharedPtr<rdr::Buffer> m_geometryBuffer;
	lib::SharedPtr<rdr::Buffer> m_primitivesBuffer;

	lib::SharedPtr<PrimitivesDS>	m_primitivesDSState;
	lib::SharedPtr<GeometryDS>		m_geometryDSState;
};

} // spt::rsc