#pragma once

#include "SculptorCoreTypes.h"
#include "ShaderStructs/ShaderStructsMacros.h"
#include "Types/Buffer.h"
#include "ShaderStructs.h"
#include "Bindless/BindlessTypes.h"


namespace spt::rsc
{

BEGIN_SHADER_STRUCT(UnifiedGeometryBuffer)
	SHADER_STRUCT_FIELD(gfx::ByteBufferRef, geometryData)
END_SHADER_STRUCT();


class GeometryManager
{
public:

	static GeometryManager& Get();

	rhi::RHIVirtualAllocation CreateGeometry(const Byte* geometryData, Uint64 dataSize);
	rhi::RHIVirtualAllocation CreateGeometry(Uint64 dataSize);

	const UnifiedGeometryBuffer& GetUnifiedGeometryBuffer() const { return m_unifiedGeometryBuffer; }

	Uint64 GetGeometryBufferDeviceAddress() const;

private:

	GeometryManager();
	void DestroyResources();

	lib::SharedPtr<rdr::Buffer> m_geometryBuffer;

	UnifiedGeometryBuffer m_unifiedGeometryBuffer;
};

} // spt::rsc
