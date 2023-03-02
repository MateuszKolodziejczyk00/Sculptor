#pragma once

#include "RHIMacros.h"
#include "SculptorCoreTypes.h"
#include "RHICore/RHIAccelerationStructureTypes.h"
#include "Vulkan/VulkanCore.h"
#include "Vulkan/Debug/DebugUtils.h"
#include "RHIBuffer.h"


namespace spt::vulkan
{

class RHI_API RHIAccelerationStructure
{
public:

	RHIAccelerationStructure();

	Bool IsValid() const;

	void						SetName(const lib::HashedString& name);
	const lib::HashedString&	GetName() const;

	VkAccelerationStructureKHR GetHandle() const;

	Uint64 GetBuildScratchSize() const;

protected:

	VkAccelerationStructureKHR m_handle;

	Uint64 m_buildScratchSize;

	DebugName m_name;
};


class RHI_API RHIBottomLevelAS : public RHIAccelerationStructure
{
public:

	RHIBottomLevelAS();

	void InitializeRHI(const rhi::BLASDefinition& definition, INOUT RHIBuffer& accelerationStructureBuffer, INOUT Uint64& accelerationStructureBufferOffset);
	void ReleaseRHI();

	VkAccelerationStructureBuildGeometryInfoKHR CreateBuildGeometryInfo(OUT VkAccelerationStructureGeometryKHR& geometry) const;

private:

	VkAccelerationStructureGeometryKHR CreateGeometryData() const;

	Uint64					m_locationsDeviceAddress;
	Uint32					m_maxVerticesNum;
	Uint32					m_indicesNum;
	Uint64					m_indicesDeviceAddress;
	VkGeometryFlagsKHR		m_geometryFlags;
};


class RHI_API RHITopLevelAS : public RHIAccelerationStructure
{
public:

	RHITopLevelAS();

	void InitializeRHI(const rhi::TLASDefinition& definition, INOUT RHIBuffer& accelerationStructureBuffer, INOUT Uint64& accelerationStructureBufferOffset);
	void ReleaseRHI();

	VkAccelerationStructureBuildGeometryInfoKHR CreateBuildGeometryInfo(OUT VkAccelerationStructureGeometryKHR& geometry) const;

	void ClearInstancesBuildBuffer();

private:

	VkAccelerationStructureGeometryKHR CreateGeometryData() const;
	
	RHIBuffer m_instancesBuildBuffer;
};

} // spt::vulkan