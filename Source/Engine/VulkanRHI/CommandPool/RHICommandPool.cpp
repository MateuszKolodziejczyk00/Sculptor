#include "RHICommandPool.h"
#include "VulkanRHI.h"

namespace spt::vulkan
{

RHICommandPool::RHICommandPool()
	: m_poolHandle(VK_NULL_HANDLE)
	, m_isLocked(false)
{ }

void RHICommandPool::InitializeRHI(Uint32 queueFamilyIdx, VkCommandPoolCreateFlags flags)
{
	SPT_PROFILE_FUNCTION();

	VkCommandPoolCreateInfo poolInfo{ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
	poolInfo.queueFamilyIndex		= queueFamilyIdx;
	poolInfo.flags					= flags | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

	SPT_VK_CHECK(vkCreateCommandPool(VulkanRHI::GetDeviceHandle(), &poolInfo, VulkanRHI::GetAllocationCallbacks(), &m_poolHandle));
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

Bool RHICommandPool::TryLock()
{
	const Bool prev = m_isLocked.exchange(true);
	return prev == false;
}

void RHICommandPool::Unlock()
{
	m_isLocked.store(false);
}

}
