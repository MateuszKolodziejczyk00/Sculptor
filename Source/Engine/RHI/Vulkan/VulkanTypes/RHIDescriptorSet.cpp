#include "RHIDescriptorSet.h"

namespace spt::vulkan
{

RHIDescriptorSet::RHIDescriptorSet()
	: m_handle(VK_NULL_HANDLE)
	, m_layout(VK_NULL_HANDLE)
{ }

RHIDescriptorSet::RHIDescriptorSet(VkDescriptorSet handle, VkDescriptorSetLayout layout)
	: m_handle(handle)
	, m_layout(layout)
{
	SPT_CHECK(m_handle != VK_NULL_HANDLE);
	SPT_CHECK(m_layout != VK_NULL_HANDLE);
}

Bool RHIDescriptorSet::IsValid() const
{
	return m_handle != VK_NULL_HANDLE && m_layout != VK_NULL_HANDLE;
}

VkDescriptorSet RHIDescriptorSet::GetHandle() const
{
	return m_handle;
}

VkDescriptorSetLayout RHIDescriptorSet::GetLayoutHandle() const
{
	return m_layout;
}

} // spt::vulkan
