#pragma once

#include "VulkanRHIMacros.h"
#include "Vulkan.h"
#include "SculptorCoreTypes.h"
#include "RHICommandBufferTypes.h"


namespace spt::rhi
{
struct RHIInitializationInfo;
struct RHIWindowInitializationInfo;
struct SubmitBatchData;
}


namespace spt::vulkan
{

class MemoryManager;
class LogicalDevice;
class CommandPoolsManager;
class LayoutsManager;


class VULKAN_RHI_API VulkanRHI
{
public:

	// RHI Interface ===================================================================

	static void Initialize(const rhi::RHIInitializationInfo& initInfo);

	static void InitializeGPUForWindow();

	static void Uninitialize();

	static void SubmitCommands(rhi::ECommandBufferQueueType queueType, const lib::DynamicArray<rhi::SubmitBatchData>& submitBatches);

	static void WaitIdle();

	// Vulkan Getters ==================================================================

	static VkInstance						GetInstanceHandle();
	static VkDevice							GetDeviceHandle();
	static VkPhysicalDevice					GetPhysicalDeviceHandle();

	static CommandPoolsManager&				GetCommandPoolsManager();

	static LayoutsManager&					GetLayoutsManager();

	static VkSurfaceKHR						GetSurfaceHandle();

	static const LogicalDevice&				GetLogicalDevice();

	static MemoryManager&					GetMemoryManager();

	static VmaAllocator						GetAllocatorHandle();

	static void								SetSurfaceHandle(VkSurfaceKHR surface);

	static const VkAllocationCallbacks*		GetAllocationCallbacks();
};

}