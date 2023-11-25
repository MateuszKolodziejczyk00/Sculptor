#pragma once

#include "SculptorCoreTypes.h"
#include "RHICore/RHIAllocationTypes.h"
#include "Vulkan/VulkanCore.h"


namespace spt::vulkan
{


namespace memory_utils
{

VmaVirtualAllocationCreateFlags GetVMAVirtualAllocationFlags(rhi::EVirtualAllocationFlags rhiFlags);

VmaMemoryUsage                  GetVMAMemoryUsage(rhi::EMemoryUsage memoryUsage);
VmaAllocationCreateFlags        GetVMAAllocationFlags(rhi::EAllocationFlags flags);
VmaVirtualBlockCreateFlags      GetVMAVirtualBlockFlags(rhi::EVirtualAllocatorFlags flags);

VmaAllocationCreateInfo         CreateAllocationInfo(const rhi::RHIAllocationInfo& rhiAllocationInfo);

std::optional<rhi::RHIAllocationInfo> GetAllocationInfo(const rhi::RHIResourceAllocationDefinition& allocationDefinition);

VmaAllocation                         GetVMAAllocation(const rhi::RHICommittedAllocation& allocation);
VmaAllocation                         GetVMAAllocation(const rhi::RHIPlacedAllocation& allocation);
VmaAllocation                         GetVMAAllocation(const rhi::RHIResourceAllocationHandle& allocation);

} // memory_utils


class VulkanVirtualAllocator
{
public:

	VulkanVirtualAllocator();


	void InitializeRHI(Uint64 memorySize, rhi::EVirtualAllocatorFlags flags = rhi::EVirtualAllocatorFlags::Default);
	void ReleaseRHI();

	Bool IsValid() const;

	Uint64 GetSize() const;

	rhi::RHIVirtualAllocation Allocate(const rhi::VirtualAllocationDefinition& definition);
	void Free(const rhi::RHIVirtualAllocation& allocation);

private:

	VmaVirtualBlock m_vitualBlock;

	Uint64                      m_size;
	rhi::EVirtualAllocatorFlags m_flags;
};

} // spt::vulkan
