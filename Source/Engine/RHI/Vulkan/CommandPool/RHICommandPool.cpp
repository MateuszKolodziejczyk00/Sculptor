#include "RHICommandPool.h"
#include "Vulkan/VulkanRHI.h"

namespace spt::vulkan
{

RHICommandPool::RHICommandPool()
	: m_poolHandle(VK_NULL_HANDLE)
	, m_isLocked(false)
	, m_acquiredBuffersNum(0)
	, m_releasedBuffersNum(0)
{ }

void RHICommandPool::InitializeRHI(Uint32 queueFamilyIdx, VkCommandPoolCreateFlags flags, VkCommandBufferLevel level)
{
	SPT_PROFILE_FUNCTION();

	VkCommandPoolCreateInfo poolInfo{ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
	poolInfo.queueFamilyIndex		= queueFamilyIdx;
	poolInfo.flags					= flags;

	SPT_VK_CHECK(vkCreateCommandPool(VulkanRHI::GetDeviceHandle(), &poolInfo, VulkanRHI::GetAllocationCallbacks(), &m_poolHandle));

	AllocateCommandBuffers(16, level);
}

void RHICommandPool::ReleaseRHI()
{
	SPT_PROFILE_FUNCTION();

	SPT_CHECK(!!IsValid());
	SPT_CHECK(!IsLocked());

	vkDestroyCommandPool(VulkanRHI::GetDeviceHandle(), m_poolHandle, VulkanRHI::GetAllocationCallbacks());

	m_poolHandle = VK_NULL_HANDLE;
}

Bool RHICommandPool::IsValid() const
{
	return m_poolHandle != VK_NULL_HANDLE;
}

VkCommandPool RHICommandPool::GetHandle() const
{
	return m_poolHandle;
}

Bool RHICommandPool::IsLocked() const
{
	return m_isLocked;
}

void RHICommandPool::ForceLock()
{
	const Bool prev = m_isLocked.exchange(true);
	SPT_CHECK(prev == false);
}

Bool RHICommandPool::TryLock()
{
	Bool success = false;

	if (HasAvailableCommandBuffers())
	{
		const Bool prev = m_isLocked.exchange(true);
		success = (prev == false);
	}

	return success;
}

void RHICommandPool::Unlock()
{
	m_isLocked.store(false);
}

VkCommandBuffer RHICommandPool::AcquireCommandBuffer()
{
	SPT_CHECK(IsLocked());

	SPT_CHECK(HasAvailableCommandBuffers());
	
	return m_commandBuffers[m_acquiredBuffersNum++];
}

Bool RHICommandPool::HasAvailableCommandBuffers() const
{
	return m_acquiredBuffersNum < m_commandBuffers.size();
}

void RHICommandPool::ReleaseCommandBuffer(VkCommandBuffer cmdBuffer)
{
	SPT_PROFILE_FUNCTION();

	++m_releasedBuffersNum;

	SPT_CHECK(m_releasedBuffersNum <= m_acquiredBuffersNum);

	if (m_releasedBuffersNum == m_commandBuffers.size())
	{
		ResetCommandPool();
	}
}

void RHICommandPool::AllocateCommandBuffers(Uint32 commandBuffersNum, VkCommandBufferLevel level)
{
	SPT_PROFILE_FUNCTION();

	VkCommandBufferAllocateInfo allocateInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
    allocateInfo.commandPool = m_poolHandle;
    allocateInfo.level = level;
	allocateInfo.commandBufferCount = commandBuffersNum;

	m_commandBuffers.resize(static_cast<SizeType>(commandBuffersNum));

	SPT_VK_CHECK(vkAllocateCommandBuffers(VulkanRHI::GetDeviceHandle(), &allocateInfo, m_commandBuffers.data()));

	m_acquiredBuffersNum = 0;
	m_releasedBuffersNum = 0;
}

void RHICommandPool::ResetCommandPool()
{
	vkResetCommandPool(VulkanRHI::GetDeviceHandle(), m_poolHandle, 0);

	m_acquiredBuffersNum = 0;
	m_releasedBuffersNum = 0;
}

}