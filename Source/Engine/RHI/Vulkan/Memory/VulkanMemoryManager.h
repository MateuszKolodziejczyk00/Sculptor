#pragma once

#include "SculptorCoreTypes.h"
#include "Vulkan/VulkanCore.h"

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

private:

	VmaAllocator m_allocator;
};

}
