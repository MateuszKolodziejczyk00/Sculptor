#include "DescriptorPool.h"
#include "Vulkan/VulkanRHI.h"

namespace spt::vulkan
{

DescriptorPool::DescriptorPool()
	: m_poolHandle(VK_NULL_HANDLE)
{ }

void DescriptorPool::Initialize(VkDescriptorPoolCreateFlags flags, Uint32 maxSetsNum, const VkDescriptorPoolSize* poolSizes, Uint32 poolSizesNum)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(!IsValid());

	VkDescriptorPoolCreateInfo poolInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
	poolInfo.flags         = flags;
    poolInfo.maxSets       = maxSetsNum;
    poolInfo.poolSizeCount = poolSizesNum;
    poolInfo.pPoolSizes    = poolSizes;

	SPT_VK_CHECK(vkCreateDescriptorPool(VulkanRHI::GetDeviceHandle(), &poolInfo, VulkanRHI::GetAllocationCallbacks(), &m_poolHandle));
}

void DescriptorPool::Release()
{
	SPT_CHECK(IsValid());

	vkDestroyDescriptorPool(VulkanRHI::GetDeviceHandle(), m_poolHandle, VulkanRHI::GetAllocationCallbacks());

	m_poolHandle = VK_NULL_HANDLE;
}

Bool DescriptorPool::IsValid() const
{
	return m_poolHandle != VK_NULL_HANDLE;
}

VkDescriptorPool DescriptorPool::GetHandle() const
{
	return m_poolHandle;
}

Bool DescriptorPool::AllocateDescriptorSets(const VkDescriptorSetLayout* layouts, Uint32 layoutsNum, lib::DynamicArray<VkDescriptorSet>& outDescriptorSets)
{
	SPT_CHECK(IsValid());

	const SizeType layoutsNumST = static_cast<SizeType>(layoutsNum);
	if (outDescriptorSets.size() < layoutsNumST)
	{
		outDescriptorSets.resize(layoutsNumST);
	}

	VkDescriptorSetAllocateInfo allocateInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
    allocateInfo.descriptorPool = m_poolHandle;
    allocateInfo.descriptorSetCount = layoutsNum;
    allocateInfo.pSetLayouts = layouts;

	const VkResult allocateResult = vkAllocateDescriptorSets(VulkanRHI::GetDeviceHandle(), &allocateInfo, outDescriptorSets.data());

	return allocateResult == VK_SUCCESS;
}

void DescriptorPool::FreeDescriptorSets(const lib::DynamicArray<VkDescriptorSet>& descriptorSets)
{
	vkFreeDescriptorSets(VulkanRHI::GetDeviceHandle(), m_poolHandle, static_cast<Uint32>(descriptorSets.size()), descriptorSets.data());
}

VkDescriptorSet DescriptorPool::AllocateDescriptorSet(VkDescriptorSetLayout layout)
{
	SPT_CHECK(IsValid());

	VkDescriptorSetAllocateInfo allocateInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
	allocateInfo.descriptorPool     = m_poolHandle;
	allocateInfo.descriptorSetCount = 1;
	allocateInfo.pSetLayouts        = &layout;

	VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
	SPT_VK_CHECK(vkAllocateDescriptorSets(VulkanRHI::GetDeviceHandle(), &allocateInfo, &descriptorSet));

	return descriptorSet;
}

void DescriptorPool::FreeDescriptorSet(VkDescriptorSet descriptorSet)
{
	vkFreeDescriptorSets(VulkanRHI::GetDeviceHandle(), m_poolHandle, 1, &descriptorSet);
}

void DescriptorPool::ResetPool()
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(IsValid());

	vkResetDescriptorPool(VulkanRHI::GetDeviceHandle(), m_poolHandle, 0);
}

} // spt::vulkan
