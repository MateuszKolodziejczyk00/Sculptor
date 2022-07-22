#pragma once

#include "VulkanRHIMacros.h"
#include "Vulkan.h"


namespace spt::rhicore
{
struct RHIInitializationInfo;
}


namespace spt::vulkan
{

class VULKANRHI_API VulkanRHI
{
public:

	// RHI Interface ===================================================================

	static void Initialize(const rhicore::RHIInitializationInfo& InitInfo);

	static void SelectAndInitializeGPU();

	static void Uninitialize();


	// Vulkan Getters ==================================================================

	static VkInstance						GetInstanceHandle();
	static VkDevice							GetDeviceHandle();

	static VmaAllocator						GetAllocatorHandle();

	static void								SetSurfaceHandle(VkSurfaceKHR surface);

	static const VkAllocationCallbacks*		GetAllocationCallbacks();
};

}