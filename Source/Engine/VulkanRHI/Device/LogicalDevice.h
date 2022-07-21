#pragma once

#include "Vulkan.h"
#include "SculptorCoreTypes.h"


namespace spt::vulkan
{

class LogicalDevice
{
public:

	LogicalDevice();

	void CreateDevice(VkPhysicalDevice physicalDevice, const VkAllocationCallbacks* allocator);
	void Destroy(const VkAllocationCallbacks* allocator);

	bool IsValid() const;

	VkDevice GetHandle();

private:

	lib::DynamicArray<VkDeviceQueueCreateInfo> CreateQueueInfos(VkPhysicalDevice physicalDevice);

	VkDevice m_deviceHandle;

	Uint32 m_gfxFamilyIdx;
	Uint32 m_asyncComputeFamilyIdx;
	Uint32 m_transferFamilyIdx;
};

}