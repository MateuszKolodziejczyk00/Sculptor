#pragma once

#include "Vulkan/VulkanCore.h"
#include "SculptorCoreTypes.h"
#include "RHICore/RHICommandBufferTypes.h"
#include "Vulkan/VulkanTypes/RHIDeviceQueue.h"
#include "RHICore/RHIDescriptorTypes.h"


namespace spt::vulkan
{

class LogicalDevice
{
public:

	LogicalDevice();

	void		CreateDevice(VkPhysicalDevice physicalDevice, const VkAllocationCallbacks* allocator);
	void		Destroy(const VkAllocationCallbacks* allocator);

	bool		IsValid() const;

	VkDevice	GetHandle() const;

	void		WaitIdle() const;

	RHIDeviceQueue GetGfxQueue() const;
	RHIDeviceQueue GetAsyncComputeQueue() const;
	RHIDeviceQueue GetTransferQueue() const;
	RHIDeviceQueue GetQueue(rhi::EDeviceCommandQueueType queueType) const;

	Uint32 GetGfxQueueFamilyIdx() const;
	Uint32 GetAsyncComputeQueueFamilyIdx() const;
	Uint32 GetTransferQueueFamilyIdx() const;
	Uint32 GetQueueFamilyIdx(rhi::EDeviceCommandQueueType queueType) const;

	const rhi::DescriptorProps& GetDescriptorProps() const { return m_descriptorProps; }

private:

	lib::DynamicArray<VkDeviceQueueCreateInfo> CreateQueueInfos(VkPhysicalDevice physicalDevice);

	VkDevice m_deviceHandle;

	Uint32 m_gfxFamilyIdx;
	Uint32 m_asyncComputeFamilyIdx;
	Uint32 m_transferFamilyIdx;

	RHIDeviceQueue m_gfxQueue;
	RHIDeviceQueue m_asyncComputeQueue;
	RHIDeviceQueue m_transferQueue;

	rhi::DescriptorProps m_descriptorProps{};
};

} // spt::vulkan