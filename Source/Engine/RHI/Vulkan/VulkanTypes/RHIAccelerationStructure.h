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

	void                     SetName(const lib::HashedString& name);
	const lib::HashedString& GetName() const;

	Uint64 GetMaxPrimitivesCount() const { return m_maxPrimitivesCount; }
	Uint64 GetStratchBufferSize() const  { return m_buildScratchSize; }

	VkAccelerationStructureKHR GetHandle() const;

	Uint64 GetBuildScratchSize() const;

	DeviceAddress GetDeviceAddress() const;

	void CopySRVDescriptor(Byte* dst) const;

protected:

	RHIAccelerationStructureReleaseTicket DeferredReleaseInternal();

	VkAccelerationStructureKHR m_handle = VK_NULL_HANDLE;

	Uint64 m_buildScratchSize = 0u;
	Uint32 m_maxPrimitivesCount  = 0u;

	DebugName m_name;
};


class RHI_API RHIBottomLevelAS : public RHIAccelerationStructure
{
public:

	RHIBottomLevelAS();

	void InitializeRHI(const rhi::BLASDefinition& definition, INOUT RHIBuffer& accelerationStructureBuffer, INOUT Uint64& accelerationStructureBufferOffset);
	void ReleaseRHI();

	RHIAccelerationStructureReleaseTicket DeferredReleaseRHI();

	VkAccelerationStructureBuildGeometryInfoKHR CreateBuildGeometryInfo(OUT VkAccelerationStructureGeometryKHR& geometry, const rhi::BLASBuildInfo* buildInfo) const;

private:

	VkAccelerationStructureGeometryKHR CreateGeometryData(const rhi::BLASBuildInfo* buildInfo) const;

	Uint32             m_maxVerticesNum = 0u;
	VkGeometryFlagsKHR m_geometryFlags  = 0u;

	rhi::EBLASGeometryType m_geometryType = rhi::EBLASGeometryType::Triangles;
};


class RHI_API RHITopLevelAS : public RHIAccelerationStructure
{
public:

	RHITopLevelAS();

	void InitializeRHI(const rhi::TLASDefinition& definition, INOUT RHIBuffer& accelerationStructureBuffer, INOUT Uint64& accelerationStructureBufferOffset);
	void ReleaseRHI();

	RHIAccelerationStructureReleaseTicket DeferredReleaseRHI();

	VkAccelerationStructureBuildGeometryInfoKHR CreateBuildGeometryInfo(OUT VkAccelerationStructureGeometryKHR& geometry, const rhi::TLASBuildInfo* buildInfo) const;

private:

	VkAccelerationStructureGeometryKHR CreateGeometryData(const rhi::TLASBuildInfo* buildInfo) const;
};


class RHI_API RHIASUtils
{
public:

	static Uint64 GetInstancesBufferSize(Uint32 instancesNum);

	static void CopyInstancesDefinitionsToBuffer(const RHIBuffer& instancesBuffer, lib::Span<const rhi::TLASInstanceDefinition> instanceDefs);

	static void CopyInstanceDefinitionToBuffer(const RHIMappedByteBuffer& mappedBuffer, Uint32 instanceIdx, const rhi::TLASInstanceDefinition& instanceDef);
};

} // spt::vulkan
