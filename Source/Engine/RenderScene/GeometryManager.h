#pragma once

#include "SculptorCoreTypes.h"
#include "ShaderStructs/ShaderStructsMacros.h"
#include "Types/Buffer.h"
#include "RGDescriptorSetState.h"
#include "DescriptorSetBindings/RWBufferBinding.h"


namespace spt::rsc
{

DS_BEGIN(GeometryDS, rg::RGDescriptorSetState<GeometryDS>)
	DS_BINDING(BINDING_TYPE(gfx::ByteAddressBuffer),	u_geometryData)
DS_END();


class GeometryManager
{
public:

	static GeometryManager& Get();

	rhi::RHIVirtualAllocation CreateGeometry(const Byte* geometryData, Uint64 dataSize);
	rhi::RHIVirtualAllocation CreateGeometry(Uint64 dataSize);

	const lib::MTHandle<GeometryDS>& GetGeometryDSState() const;

	Uint64 GetGeometryBufferDeviceAddress() const;

private:

	GeometryManager();
	void DestroyResources();

	lib::SharedPtr<rdr::Buffer> m_geometryBuffer;
	lib::MTHandle<GeometryDS>	m_geometryDSState;
};

} // spt::rsc