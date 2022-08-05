#include "DescriptorPool.h"
#include "VulkanRHI.h"

namespace spt::vulkan
{

DescriptorPool::DescriptorPool()
	: m_poolHandle(VK_NULL_HANDLE)
{ }

void DescriptorPool::Initialize(VkDescriptorPoolCreateFlags flags, Uint32 maxSetsNum, const lib::DynamicArray<VkDescriptorPoolSize>& poolSizes)
{
	SPT_PROFILE_FUNCTION();

	SPT_CHECK(!IsValid());

	VkDescriptorPoolCreateInfo poolInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
	poolInfo.flags = flags;
    poolInfo.maxSets = maxSetsNum;
    poolInfo.poolSizeCount = static_cast<Uint32>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();

	SPT_VK_CHECK(vkCreateDescriptorPool(VulkanRHI::GetDeviceHandle(), &poolInfo, VulkanRHI::GetAllocationCallbacks(), &m_poolHandle));
}

void DescriptorPool::Release()
{
	SPT_CHECK(IsValid());

	vkDestroyDescriptorPool(VulkanRHI::GetDeviceHandle(), m_poolHandle, VulkanRHI::GetAllocationCallbacks());
}

Bool DescriptorPool::IsValid() const
{
	return m_poolHandle != VK_NULL_HANDLE;
}

VkDescriptorPool DescriptorPool::GetHandle() const
{
	return m_poolHandle;
}

}
