#pragma once

#include "VulkanRHIMacros.h"
#include "Vulkan.h"


namespace spt::rhi
{
struct RHIInitializationInfo;
struct RHIWindowInitializationInfo;
}


namespace spt::vulkan
{

class MemoryManager;
class LogicalDevice;
class RHICommandPoolsManager;


class VULKAN_RHI_API VulkanRHI
{
public:

	// RHI Interface ===================================================================

	static void Initialize(const rhi::RHIInitializationInfo& initInfo);

	static void InitializeGPUForWindow();

	static void Uninitialize();

	// Vulkan Getters ==================================================================

	static VkInstance						GetInstanceHandle();
	static VkDevice							GetDeviceHandle();
	static VkPhysicalDevice					GetPhysicalDeviceHandle();

	static RHICommandPoolsManager&			GetCommandPoolsManager();

	static VkSurfaceKHR						GetSurfaceHandle();

	static const LogicalDevice&				GetLogicalDevice();

	static MemoryManager&					GetMemoryManager();

	static VmaAllocator						GetAllocatorHandle();

	static void								SetSurfaceHandle(VkSurfaceKHR surface);

	static const VkAllocationCallbacks*		GetAllocationCallbacks();
};

}