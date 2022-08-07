#pragma once

#include "SculptorCoreTypes.h"
#include "Vulkan/VulkanCore.h"

namespace spt::rhi
{
struct RHIAllocationInfo;
}


namespace spt::vulkan
{

class MemoryManager
{
public:

	MemoryManager();

	void						Initialize(VkInstance instance, VkDevice logicalDevice, VkPhysicalDevice physicalDevice, const VkAllocationCallbacks* allocationCallbacks);
	void						Destroy();

	Bool						IsValid() const;

	VmaAllocator				GetAllocatorHandle() const;

	VmaAllocationCreateInfo		CreateAllocationInfo(const rhi::RHIAllocationInfo& rhiAllocationInfo) const;

private:

	VmaAllocator m_allocator;
};

}
