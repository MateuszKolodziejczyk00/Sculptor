#include "PipelineLayout.h"
#include "Vulkan/VulkanRHI.h"

namespace spt::vulkan
{

PipelineLayout::PipelineLayout(const rhi::PipelineLayoutDefinition& layoutDef)
	: m_layoutHandle(VK_NULL_HANDLE)
{
	SPT_PROFILER_FUNCTION();

	lib::DynamicArray<VkDescriptorSetLayout> layoutHandles;
	layoutHandles.reserve(layoutDef.descriptorSetLayouts.size());

	std::transform(layoutDef.descriptorSetLayouts.begin(), layoutDef.descriptorSetLayouts.end(),
				   std::back_inserter(layoutHandles),
				   [](const rhi::RHIDescriptorSetLayout& layout) { return layout.GetHandle(); });


	VkPipelineLayoutCreateInfo pipelineLayoutInfo{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    pipelineLayoutInfo.setLayoutCount         = static_cast<Uint32>(layoutHandles.size());
	pipelineLayoutInfo.pSetLayouts            = layoutHandles.data();
    pipelineLayoutInfo.pushConstantRangeCount = 0;
	pipelineLayoutInfo.pPushConstantRanges    = nullptr;

	SPT_VK_CHECK(vkCreatePipelineLayout(VulkanRHI::GetDeviceHandle(), &pipelineLayoutInfo, VulkanRHI::GetAllocationCallbacks(), &m_layoutHandle));

	m_descriptorSetLayouts = layoutDef.descriptorSetLayouts;
}

PipelineLayout::~PipelineLayout()
{
	SPT_PROFILER_FUNCTION();

	vkDestroyPipelineLayout(VulkanRHI::GetDeviceHandle(), m_layoutHandle, VulkanRHI::GetAllocationCallbacks());
}

VkPipelineLayout PipelineLayout::GetHandle() const
{
	return m_layoutHandle;
}

const RHIDescriptorSetLayout& PipelineLayout::GetDescriptorSetLayout(Uint32 descriptorSetIdx) const
{
	SPT_CHECK(descriptorSetIdx < GetDescriptorSetsNum());
	return m_descriptorSetLayouts[descriptorSetIdx];
}

Uint32 PipelineLayout::GetDescriptorSetsNum() const
{
	return static_cast<Uint32>(m_descriptorSetLayouts.size());
}

} // spt::vulkan
