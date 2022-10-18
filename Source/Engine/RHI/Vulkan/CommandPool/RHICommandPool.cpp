#include "RHICommandPool.h"
#include "Vulkan/VulkanRHI.h"

namespace spt::vulkan
{

RHICommandPool::RHICommandPool()
	: m_poolHandle(VK_NULL_HANDLE)
	, m_acquiredBuffersNum(0)
{ }

void RHICommandPool::InitializeRHI(Uint32 queueFamilyIdx, VkCommandPoolCreateFlags flags, VkCommandBufferLevel level)
{
	SPT_PROFILER_FUNCTION();

	VkCommandPoolCreateInfo poolInfo{ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
	poolInfo.queueFamilyIndex		= queueFamilyIdx;
	poolInfo.flags					= flags;

	SPT_VK_CHECK(vkCreateCommandPool(VulkanRHI::GetDeviceHandle(), &poolInfo, VulkanRHI::GetAllocationCallbacks(), &m_poolHandle));

	AllocateCommandBuffers(16, level);
}

void RHICommandPool::ReleaseRHI()
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(!!IsValid());

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

VkCommandBuffer RHICommandPool::TryAcquireCommandBuffer()
{
	return HasAvailableCommandBuffers() ? m_commandBuffers[m_acquiredBuffersNum++] : VK_NULL_HANDLE;
}

void RHICommandPool::ResetCommandPool()
{
	vkResetCommandPool(VulkanRHI::GetDeviceHandle(), m_poolHandle, 0);

	m_acquiredBuffersNum = 0;
}

void RHICommandPool::AllocateCommandBuffers(Uint32 commandBuffersNum, VkCommandBufferLevel level)
{
	SPT_PROFILER_FUNCTION();

	VkCommandBufferAllocateInfo allocateInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
    allocateInfo.commandPool = m_poolHandle;
    allocateInfo.level = level;
	allocateInfo.commandBufferCount = commandBuffersNum;

	m_commandBuffers.resize(static_cast<SizeType>(commandBuffersNum));

	SPT_VK_CHECK(vkAllocateCommandBuffers(VulkanRHI::GetDeviceHandle(), &allocateInfo, m_commandBuffers.data()));

	m_acquiredBuffersNum = 0;
}

Bool RHICommandPool::HasAvailableCommandBuffers() const
{
	return m_acquiredBuffersNum < m_commandBuffers.size();
}

} // spt::vulkan
