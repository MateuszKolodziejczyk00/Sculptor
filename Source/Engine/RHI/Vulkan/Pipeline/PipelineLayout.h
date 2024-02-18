#pragma once

#include "SculptorCoreTypes.h"
#include "RHICore/RHIPipelineLayoutTypes.h"
#include "Vulkan/VulkanCore.h"


namespace spt::vulkan
{

class PipelineLayout
{
public:

	explicit PipelineLayout(const rhi::PipelineLayoutDefinition& layoutDef);
	~PipelineLayout();

	VkPipelineLayout GetHandle() const;

	SPT_NODISCARD const RHIDescriptorSetLayout& GetDescriptorSetLayout(Uint32 descriptorSetIdx) const;
	SPT_NODISCARD Uint32                        GetDescriptorSetsNum() const;

private:

	VkPipelineLayout							m_layoutHandle;
	lib::DynamicArray<RHIDescriptorSetLayout>	m_descriptorSetLayouts;
};

} // spt::vulkan
