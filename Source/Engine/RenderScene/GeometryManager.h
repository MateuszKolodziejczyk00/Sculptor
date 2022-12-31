#pragma once

#include "SculptorCoreTypes.h"
#include "ShaderStructs/ShaderStructsMacros.h"
#include "Types/Buffer.h"


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


class GeometryManager
{
public:

	static GeometryManager& Get();

	rhi::RHISuballocation CreateGeometry(const Byte* geometryData, Uint64 size);

	rhi::RHISuballocation CreatePrimitives(const PrimitiveGeometryInfo* primitivesInfo, Uint32 primitivesNum);

private:

	GeometryManager();

	lib::SharedPtr<rdr::Buffer> m_geometryBuffer;
	lib::SharedPtr<rdr::Buffer> m_primitivesBuffer;
};

} // spt::rsc