#pragma once


#include "Vulkan/Vulkan123.h"
#include "SculptorCoreTypes.h"
#include <atomic>


namespace spt::vulkan
{

class RHICommandPool
{
public:

	RHICommandPool();

	void									InitializeRHI(Uint32 queueFamilyIdx, VkCommandPoolCreateFlags flags, VkCommandBufferLevel level);
	void									ReleaseRHI();

	Bool									IsValid() const;

	VkCommandPool							GetHandle() const;

	Bool									IsLocked() const;

	void									ForceLock();
	Bool									TryLock();
	void									Unlock();

	VkCommandBuffer							AcquireCommandBuffer();
	void									ReleaseCommandBuffer(VkCommandBuffer cmdBuffer);

private:

	void									AllocateCommandBuffers(Uint32 commandBuffersNum, VkCommandBufferLevel level);

	Bool									HasAvailableCommandBuffers() const;

	void									ResetCommandPool();

	VkCommandPool							m_poolHandle;

	/** Pool must be locked for the time when any thread is recording commands to buffer allocated from this pool */
	std::atomic<Bool>						m_isLocked;

	lib::DynamicArray<VkCommandBuffer>		m_commandBuffers;
	SizeType								m_acquiredBuffersNum;
	SizeType								m_releasedBuffersNum;
};

}