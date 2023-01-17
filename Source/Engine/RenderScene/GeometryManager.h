#pragma once

#include "SculptorCoreTypes.h"
#include "ShaderStructs/ShaderStructsMacros.h"
#include "Types/Buffer.h"
#include "RGDescriptorSetState.h"
#include "DescriptorSetBindings/RWBufferBinding.h"


namespace spt::rsc
{

DS_BEGIN(, GeometryDS, rg::RGDescriptorSetState<GeometryDS>, rhi::EShaderStageFlags::Vertex)
	DS_BINDING(BINDING_TYPE(gfx::ByteAddressBuffer),	geometryData)
DS_END();


class GeometryManager
{
public:

	static GeometryManager& Get();

	rhi::RHISuballocation CreateGeometry(const Byte* geometryData, Uint64 dataSize);
	rhi::RHISuballocation CreateGeometry(Uint64 dataSize);

	const lib::SharedPtr<GeometryDS>& GetGeometryDSState() const;

private:

	GeometryManager();
	void DestroyResources();

	lib::SharedPtr<rdr::Buffer> m_geometryBuffer;
	lib::SharedPtr<GeometryDS>	m_geometryDSState;
};

} // spt::rsc