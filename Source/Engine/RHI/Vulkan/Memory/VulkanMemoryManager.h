#pragma once

#include "SculptorCoreTypes.h"
#include "Vulkan/VulkanCore.h"
#include "RHICore/RHIAllocationTypes.h"

namespace spt::rhi
{
struct RHIAllocationInfo;
}


namespace spt::vulkan
{

class VulkanMemoryManager
{
public:

	VulkanMemoryManager();

	void Initialize(VkInstance instance, VkDevice logicalDevice, VkPhysicalDevice physicalDevice, const VkAllocationCallbacks* allocationCallbacks);
	void Destroy();

	Bool IsValid() const;

	VmaAllocator GetAllocatorHandle() const;

	rhi::RHIMemoryUsageStats GetMemoryUsageStats() const;

private:

	VmaAllocator m_allocator;
};

}
