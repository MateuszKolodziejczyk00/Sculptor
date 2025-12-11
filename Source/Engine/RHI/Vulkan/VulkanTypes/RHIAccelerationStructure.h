#pragma once

#include "RHIMacros.h"
#include "SculptorCoreTypes.h"
#include "RHICore/RHIAccelerationStructureTypes.h"
#include "Vulkan/VulkanCore.h"
#include "Vulkan/Debug/DebugUtils.h"
#include "RHIBuffer.h"


namespace spt::vulkan
{

struct RHI_API RHIAccelerationStructureReleaseTicket
{
	void ExecuteReleaseRHI();

	RHIResourceReleaseTicket<VkAccelerationStructureKHR> handle;

#if SPT_RHI_DEBUG
	lib::HashedString name;
#endif // SPT_RHI_DEBUG
};


class RHI_API RHIAccelerationStructure
{
public:

	RHIAccelerationStructure();

	Bool IsValid() const;

	void						SetName(const lib::HashedString& name);
	const lib::HashedString&	GetName() const;

	VkAccelerationStructureKHR GetHandle() const;

	Uint64 GetBuildScratchSize() const;

	VkAccelerationStructureBuildRangeInfoKHR CreateBuildRangeInfo() const;

	DeviceAddress GetDeviceAddress() const;

	void CopySRVDescriptor(Byte* dst) const;

protected:

	RHIAccelerationStructureReleaseTicket DeferredReleaseInternal();

	VkAccelerationStructureKHR m_handle;

	Uint64 m_buildScratchSize;
	Uint32 m_primitivesCount;

	DebugName m_name;
};


class RHI_API RHIBottomLevelAS : public RHIAccelerationStructure
{
public:

	RHIBottomLevelAS();

	void InitializeRHI(const rhi::BLASDefinition& definition, INOUT RHIBuffer& accelerationStructureBuffer, INOUT Uint64& accelerationStructureBufferOffset);
	void ReleaseRHI();

	RHIAccelerationStructureReleaseTicket DeferredReleaseRHI();

	VkAccelerationStructureBuildGeometryInfoKHR CreateBuildGeometryInfo(OUT VkAccelerationStructureGeometryKHR& geometry) const;

private:

	VkAccelerationStructureGeometryKHR CreateGeometryData() const;

	Uint64					m_locationsDeviceAddress;
	Uint32					m_maxVerticesNum;
	Uint64					m_indicesDeviceAddress;
	VkGeometryFlagsKHR		m_geometryFlags;
};


class RHI_API RHITopLevelAS : public RHIAccelerationStructure
{
public:

	RHITopLevelAS();

	void InitializeRHI(const rhi::TLASDefinition& definition, INOUT RHIBuffer& accelerationStructureBuffer, INOUT Uint64& accelerationStructureBufferOffset, OUT RHIBuffer& instancesBuildBuffer);
	void ReleaseRHI();

	RHIAccelerationStructureReleaseTicket DeferredReleaseRHI();

	VkAccelerationStructureBuildGeometryInfoKHR CreateBuildGeometryInfo(const RHIBuffer& instancesBuildBuffer, OUT VkAccelerationStructureGeometryKHR& geometry) const;

private:

	VkAccelerationStructureGeometryKHR CreateGeometryData(const RHIBuffer& instancesBuildBuffer) const;
};

} // spt::vulkan