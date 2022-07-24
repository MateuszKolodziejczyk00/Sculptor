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


class VULKAN_RHI_API VulkanRHI
{
public:

	// RHI Interface ===================================================================

	static void Initialize(const rhi::RHIInitializationInfo& initInfo);

	static void InitializeWindow(const rhi::RHIWindowInitializationInfo& initInfo);

	static void Uninitialize();

	// Vulkan Getters ==================================================================

	static VkInstance						GetInstanceHandle();
	static VkDevice							GetDeviceHandle();
	static VkPhysicalDevice					GetPhysicalDeviceHandle();

	static VkSurfaceKHR						GetSurfaceHandle();

	static const LogicalDevice&				GetLogicalDevice();

	static MemoryManager&					GetMemoryManager();

	static VmaAllocator						GetAllocatorHandle();

	static void								SetSurfaceHandle(VkSurfaceKHR surface);

	static const VkAllocationCallbacks*		GetAllocationCallbacks();
};

}