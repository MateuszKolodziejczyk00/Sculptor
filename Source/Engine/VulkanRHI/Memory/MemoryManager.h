#pragma once

#include "SculptorCoreTypes.h"
#include "Vulkan.h"


namespace spt::vulkan
{

class MemoryManager
{
public:

	MemoryManager();

	void			Initialize(VkInstance instance, VkDevice logicalDevice, VkPhysicalDevice physicalDevice, const VkAllocationCallbacks* allocationCallbacks);
	void			Destroy();

	Bool			IsValid() const;

	VmaAllocator	GetAllocatorHandle() const;

private:

	VmaAllocator m_allocator;
};

}
