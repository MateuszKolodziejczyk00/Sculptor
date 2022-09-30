#pragma once

#include "RHIMacros.h"
#include "Vulkan/VulkanCore.h"
#include "SculptorCoreTypes.h"
#include "RHICore/RHICommandBufferTypes.h"
#include "RHICore/RHITypes.h"


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
class PipelineLayoutsManager;


class RHI_API VulkanRHI
{
public:

	// RHI Interface ===================================================================

	static void				Initialize(const rhi::RHIInitializationInfo& initInfo);

	static void				InitializeGPUForWindow();

	static void				Uninitialize();

	static void				BeginFrame();
	static void				EndFrame();

	static rhi::ERHIType	GetRHIType();

	static void				SubmitCommands(rhi::ECommandBufferQueueType queueType, const lib::DynamicArray<rhi::SubmitBatchData>& submitBatches);

	static void				WaitIdle();

	static void				EnableValidationWarnings(Bool enable);

	// Vulkan Getters ==================================================================

	static VkInstance						GetInstanceHandle();
	static VkDevice							GetDeviceHandle();
	static VkPhysicalDevice					GetPhysicalDeviceHandle();

	static CommandPoolsManager&				GetCommandPoolsManager();

	static LayoutsManager&					GetLayoutsManager();

	static PipelineLayoutsManager&			GetPipelineLayoutsManager();

	static VkSurfaceKHR						GetSurfaceHandle();

	static const LogicalDevice&				GetLogicalDevice();

	static MemoryManager&					GetMemoryManager();

	static VmaAllocator						GetAllocatorHandle();

	static void								SetSurfaceHandle(VkSurfaceKHR surface);

	static const VkAllocationCallbacks*		GetAllocationCallbacks();
};

} // spt::vulkan