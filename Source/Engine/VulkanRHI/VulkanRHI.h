#pragma once

#include "VulkanRHIMacros.h"
#include "Vulkan.h"


namespace spt::rhi
{
struct RHIInitializationInfo;
}


namespace spt::vulkan
{

class MemoryManager;


class VULKAN_RHI_API VulkanRHI
{
public:

	// RHI Interface ===================================================================

	static void Initialize(const rhi::RHIInitializationInfo& InitInfo);

	static void SelectAndInitializeGPU();

	static void Uninitialize();


	// Vulkan Getters ==================================================================

	static VkInstance						GetInstanceHandle();
	static VkDevice							GetDeviceHandle();

	static MemoryManager&					GetMemoryManager();

	static VmaAllocator						GetAllocatorHandle();

	static void								SetSurfaceHandle(VkSurfaceKHR surface);

	static const VkAllocationCallbacks*		GetAllocationCallbacks();
};

}