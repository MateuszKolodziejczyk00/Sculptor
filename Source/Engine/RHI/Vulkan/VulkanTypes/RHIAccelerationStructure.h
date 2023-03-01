#pragma once

#include "RHIMacros.h"
#include "SculptorCoreTypes.h"
#include "RHICore/RHIAccelerationStructureTypes.h"
#include "Vulkan/VulkanCore.h"
#include "Vulkan/Debug/DebugUtils.h"


namespace spt::vulkan
{

class RHIBuffer;


class RHI_API RHIAccelerationStructure
{
public:

	RHIAccelerationStructure();

	void ReleaseRHI();

	Bool IsValid() const;

	void						SetName(const lib::HashedString& name);
	const lib::HashedString&	GetName() const;

	VkAccelerationStructureKHR GetHandle() const;

protected:

	VkAccelerationStructureKHR m_handle;

	DebugName m_name;
};


class RHI_API RHIBottomLevelAS : public RHIAccelerationStructure
{
public:

	RHIBottomLevelAS();

	void InitializeRHI(const rhi::BLASDefinition& definition, INOUT RHIBuffer& accelerationStructureBuffer, INOUT Uint64& accelerationStructureBufferOffset);

private:

	VkAccelerationStructureGeometryKHR CreateGeometryData() const;

	Uint64					m_locationsDeviceAddress;
	Uint32					m_maxVerticesNum;
	Uint32					m_indicesNum;
	Uint64					m_indicesDeviceAddress;
	VkGeometryFlagsKHR		m_geometryFlags;
};

} // spt::vulkan