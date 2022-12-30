#pragma once

#include "GraphicsMacros.h"
#include "SculptorCoreTypes.h"
#include "UnifiedBuffer.h"
#include "ShaderStructs/ShaderStructsMacros.h"


namespace spt::gfx
{

BEGIN_SHADER_STRUCT(, PrimitiveGeometryInfo)
	SHADER_STRUCT_FIELD(Uint32, indexBufferOffset)
	SHADER_STRUCT_FIELD(Uint32, indicesNum)
	SHADER_STRUCT_FIELD(Uint32, locationsOffset)
	SHADER_STRUCT_FIELD(Uint32, normalsOffset)
	SHADER_STRUCT_FIELD(Uint32, tangentsOffset)
	SHADER_STRUCT_FIELD(Uint32, uvsOffset)
	SHADER_STRUCT_FIELD(Uint32, padding0)
	SHADER_STRUCT_FIELD(Uint32, padding1)
END_SHADER_STRUCT();


class GRAPHICS_API GeometryManager
{
public:

	static GeometryManager& Get();

	rhi::RHISuballocation CreateGeometry(const Byte* geometryData, Uint64 size);

	rhi::RHISuballocation CreatePrimitives(const PrimitiveGeometryInfo* primitivesInfo, Uint32 primitivesNum);

private:

	GeometryManager();

	UnifiedBuffer m_geometryBuffer;
	UnifiedBuffer m_primitivesBuffer;
};

} // spt::gfx