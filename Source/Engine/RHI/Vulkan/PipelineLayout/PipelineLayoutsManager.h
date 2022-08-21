#pragma once

#include "SculptorCoreTypes.h"
#include "RHICore/RHIPipelineLayoutTypes.h"
#include "Vulkan/VulkanCore.h"


namespace spt::vulkan
{

class PipelineLayoutsManager
{
public:

	PipelineLayoutsManager() = default;

	void				InitializeRHI();
	void				ReleaseRHI();

	VkPipelineLayout	GetOrCreatePipelineLayout(const rhi::PipelineLayoutDefinition& definition);

private:

	VkDescriptorSetLayout	GetOrCreateDSLayout(const rhi::DescriptorSetDefinition& dsDef);
	VkDescriptorSetLayout	CreateDSLayout(const rhi::DescriptorSetDefinition& dsDef) const;

	VkPipelineLayout		CreatePipelineLayout(const rhi::PipelineLayoutDefinition& definition);

	lib::HashMap<SizeType, VkPipelineLayout>		m_cachedPipelineLayouts;
	lib::HashMap<SizeType, VkDescriptorSetLayout>	m_cachedDSLayouts;
};

} // spt::vulkan
