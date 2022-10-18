#pragma once


#include "Vulkan/VulkanCore.h"
#include "SculptorCoreTypes.h"
#include <atomic>


namespace spt::vulkan
{

class RHICommandPool
{
public:

	RHICommandPool();

	void				InitializeRHI(Uint32 queueFamilyIdx, VkCommandPoolCreateFlags flags, VkCommandBufferLevel level);
	void				ReleaseRHI();

	Bool				IsValid() const;

	VkCommandPool		GetHandle() const;

	VkCommandBuffer		TryAcquireCommandBuffer();

	void				ResetCommandPool();

private:

	void				AllocateCommandBuffers(Uint32 commandBuffersNum, VkCommandBufferLevel level);

	Bool				HasAvailableCommandBuffers() const;

	VkCommandPool							m_poolHandle;
	lib::DynamicArray<VkCommandBuffer>		m_commandBuffers;
	SizeType								m_acquiredBuffersNum;
	SizeType								m_releasedBuffersNum;

};

} // spt::vulkan