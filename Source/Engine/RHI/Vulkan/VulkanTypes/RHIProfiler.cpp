#include "RHIProfiler.h"
#include "Vulkan/VulkanCore.h"
#include "Vulkan/VulkanRHI.h"
#include "Vulkan/Device/LogicalDevice.h"
#include "RHICommandBuffer.h"
#include "RHIWindow.h"

namespace spt::vulkan
{

namespace impl
{

#if WITH_OPTICK

class GPUProfiler
{
public:

	static void Initialize()
	{
		const LogicalDevice& logicalDevice = VulkanRHI::GetLogicalDevice();

		const VkDevice device = logicalDevice.GetHandle();
		const VkPhysicalDevice physicalDevice = VulkanRHI::GetPhysicalDeviceHandle();

		VkDevice devices[] = {
			device,
			device,
			device
		};

		VkPhysicalDevice physicalDevices[] = {
			physicalDevice,
			physicalDevice,
			physicalDevice
		};

		VkQueue queues[] = {
			logicalDevice.GetGfxQueueHandle(),
			logicalDevice.GetAsyncComputeQueueHandle(),
			logicalDevice.GetTransferQueueHandle()
		};

		Uint32 queueFamilies[] = {
			logicalDevice.GetGfxQueueFamilyIdx(),
			logicalDevice.GetAsyncComputeQueueFamilyIdx(),
			logicalDevice.GetTransferQueueFamilyIdx()
		};

		const Uint32 queuesNum = SPT_ARRAY_SIZE(queues);
		SPT_CHECK(queuesNum == SPT_ARRAY_SIZE(devices));
		SPT_CHECK(queuesNum == SPT_ARRAY_SIZE(physicalDevices));
		SPT_CHECK(queuesNum == SPT_ARRAY_SIZE(queueFamilies));

		const Optick::VulkanFunctions functionsTable = BuildVulkanFunctionsTable();

		OPTICK_GPU_INIT_VULKAN(devices, physicalDevices, queues, queueFamilies, queuesNum, &functionsTable);
	}

	static void FlipFrame(const RHIWindow& window)
	{
		const VkSwapchainKHR swapchain = window.GetSwapchainHandle();
		OPTICK_GPU_FLIP(swapchain);
	}

	static RHIProfiler::CmdBufferContext GetCommandBufferContext(const RHICommandBuffer& cmdBuffer)
	{
		// in case of optick context is handle to vulkan command buffer
		static_assert(sizeof(VkCommandBuffer) == sizeof(RHIProfiler::CmdBufferContext));
		return reinterpret_cast<RHIProfiler::CmdBufferContext>(cmdBuffer.GetHandle());
	}

private:

	static Optick::VulkanFunctions BuildVulkanFunctionsTable()
	{
		Optick::VulkanFunctions functionsTable{};
		functionsTable.vkGetPhysicalDeviceProperties	= vkGetPhysicalDeviceProperties;
		functionsTable.vkCmdResetQueryPool				= vkCmdResetQueryPool;
		functionsTable.vkDestroyCommandPool				= vkDestroyCommandPool;
		functionsTable.vkDestroyQueryPool				= vkDestroyQueryPool;
		functionsTable.vkDestroyFence					= vkDestroyFence;
		functionsTable.vkFreeCommandBuffers				= vkFreeCommandBuffers;

		// These functions need to be casted, because their return type is different (as VkResult is enum)
		// Optick functions returns Int32, so calling them will be safe
		functionsTable.vkCreateQueryPool		= reinterpret_cast<PFN_vkCreateQueryPool_>(vkCreateQueryPool);
		functionsTable.vkCreateCommandPool		= reinterpret_cast<PFN_vkCreateCommandPool_>(vkCreateCommandPool);
		functionsTable.vkAllocateCommandBuffers	= reinterpret_cast<PFN_vkAllocateCommandBuffers_>(vkAllocateCommandBuffers);
		functionsTable.vkCreateFence			= reinterpret_cast<PFN_vkCreateFence_>(vkCreateFence);
		functionsTable.vkQueueSubmit			= reinterpret_cast<PFN_vkQueueSubmit_>(vkQueueSubmit);
		functionsTable.vkWaitForFences			= reinterpret_cast<PFN_vkWaitForFences_>(vkWaitForFences);
		functionsTable.vkResetCommandBuffer		= reinterpret_cast<PFN_vkResetCommandBuffer_>(vkResetCommandBuffer);
		functionsTable.vkCmdWriteTimestamp		= reinterpret_cast<PFN_vkCmdWriteTimestamp_>(vkCmdWriteTimestamp);
		functionsTable.vkGetQueryPoolResults	= reinterpret_cast<PFN_vkGetQueryPoolResults_>(vkGetQueryPoolResults);
		functionsTable.vkBeginCommandBuffer		= reinterpret_cast<PFN_vkBeginCommandBuffer_>(vkBeginCommandBuffer);
		functionsTable.vkEndCommandBuffer		= reinterpret_cast<PFN_vkEndCommandBuffer_>(vkEndCommandBuffer);
		functionsTable.vkResetFences			= reinterpret_cast<PFN_vkResetFences_>(vkResetFences);

		return functionsTable;
	}
};

#else

// Null implementation
class GPUProfiler
{
public:

	static void								Initialize() {}

	static void								FlipFrame(const RHIWindow& window) {}

	static RHIProfiler::CmdBufferContext	GetCommandBufferContext(const RHICommandBuffer& cmdBuffer) { return RHIProfiler::CmdBufferContext{}; }

};

#endif // WITH_OPTICK


} // impl

void RHIProfiler::Initialize()
{
	impl::GPUProfiler::Initialize();
}

void RHIProfiler::FlipFrame(const RHIWindow& window)
{
	impl::GPUProfiler::FlipFrame(window);
}

RHIProfiler::CmdBufferContext RHIProfiler::GetCommandBufferContext(const RHICommandBuffer& cmdBuffer)
{
	return impl::GPUProfiler::GetCommandBufferContext(cmdBuffer);
}

} // spt::vulkan
