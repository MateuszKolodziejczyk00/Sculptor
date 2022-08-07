#pragma once

#include "Vulkan/Vulkan123.h"
#include "SculptorCoreTypes.h"
#include "RHICore/RHICommandBufferTypes.h"


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

	VkQueue		GetGfxQueueHandle() const;
	VkQueue		GetAsyncComputeQueueHandle() const;
	VkQueue		GetTransferQueueHandle() const;

	VkQueue		GetQueueHandle(rhi::ECommandBufferQueueType queueType) const;

	Uint32		GetGfxQueueIdx() const;
	Uint32		GetAsyncComputeQueueIdx() const;
	Uint32		GetTransferQueueIdx() const;

	Uint32		GetQueueIdx(rhi::ECommandBufferQueueType queueType) const;

private:

	lib::DynamicArray<VkDeviceQueueCreateInfo> CreateQueueInfos(VkPhysicalDevice physicalDevice);

	VkDevice	m_deviceHandle;

	Uint32		m_gfxFamilyIdx;
	Uint32		m_asyncComputeFamilyIdx;
	Uint32		m_transferFamilyIdx;

	VkQueue		m_gfxQueueHandle;
	VkQueue		m_asyncComputeQueueHandle;
	VkQueue		m_transferQueueHandle;
};

}