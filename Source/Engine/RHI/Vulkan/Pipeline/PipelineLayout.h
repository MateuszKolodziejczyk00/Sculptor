#pragma once

#include "SculptorCoreTypes.h"
#include "RHICore/RHIPipelineLayoutTypes.h"
#include "Vulkan/VulkanCore.h"


namespace spt::vulkan
{

struct VulkanPipelineLayoutDefinition
{
	VulkanPipelineLayoutDefinition() = default;

	lib::DynamicArray<VkDescriptorSetLayout> descriptorSetLayouts;
};


class PipelineLayout
{
public:

	explicit PipelineLayout(const VulkanPipelineLayoutDefinition& layoutDef);
	~PipelineLayout();

	VkPipelineLayout	GetHandle() const;

	SPT_NODISCARD VkDescriptorSetLayout	GetDescriptorSetLayout(Uint32 descriptorSetIdx) const;
	SPT_NODISCARD Uint32				GetDescriptorSetsNum() const;

	SPT_NODISCARD const lib::DynamicArray<VkDescriptorSetLayout>& GetDescriptorSetLayouts() const;

private:

	VkPipelineLayout							m_layoutHandle;
	lib::DynamicArray<VkDescriptorSetLayout>	m_descriptorSetLayouts;
};

} // spt::vulkan
