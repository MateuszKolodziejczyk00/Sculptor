#include "PipelineLayout.h"
#include "Vulkan/VulkanRHI.h"

namespace spt::vulkan
{

PipelineLayout::PipelineLayout(const VulkanPipelineLayoutDefinition& layoutDef)
	: m_layoutHandle(VK_NULL_HANDLE)
{
	SPT_PROFILE_FUNCTION();

	VkPipelineLayoutCreateInfo pipelineLayoutInfo{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    pipelineLayoutInfo.setLayoutCount			= static_cast<Uint32>(layoutDef.descriptorSetLayouts.size());
    pipelineLayoutInfo.pSetLayouts				= layoutDef.descriptorSetLayouts.data();
    pipelineLayoutInfo.pushConstantRangeCount	= 0;
	pipelineLayoutInfo.pPushConstantRanges		= nullptr;

	SPT_VK_CHECK(vkCreatePipelineLayout(VulkanRHI::GetDeviceHandle(), &pipelineLayoutInfo, VulkanRHI::GetAllocationCallbacks(), &m_layoutHandle));

	m_descriptorSetLayouts = layoutDef.descriptorSetLayouts;
}

PipelineLayout::~PipelineLayout()
{
	SPT_PROFILE_FUNCTION();

	vkDestroyPipelineLayout(VulkanRHI::GetDeviceHandle(), m_layoutHandle, VulkanRHI::GetAllocationCallbacks());
}

VkPipelineLayout PipelineLayout::GetHandle() const
{
	return m_layoutHandle;
}

VkDescriptorSetLayout PipelineLayout::GetDescriptorSetLayout(Uint32 descriptorSetIdx) const
{
	// it's kinda odd to use Uint32 as idx here, but we use it here because it's used in vulkan api interface
	const SizeType idxAsSizeType = static_cast<SizeType>(descriptorSetIdx);
	return idxAsSizeType < m_descriptorSetLayouts.size() ? m_descriptorSetLayouts[idxAsSizeType] : VK_NULL_HANDLE;
}

Uint32 PipelineLayout::GetDescriptorSetsNum() const
{
	return static_cast<Uint32>(m_descriptorSetLayouts.size());
}

const lib::DynamicArray<VkDescriptorSetLayout>& PipelineLayout::GetDescriptorSetLayouts() const
{
	return m_descriptorSetLayouts;
}

} // spt::vulkan
