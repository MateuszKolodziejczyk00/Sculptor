#pragma once

#include "SculptorCoreTypes.h"
#include "Vulkan/VulkanCore.h"
#include "PipelineLayout.h"


namespace spt::vulkan
{

class PipelineLayoutsManager
{
public:

	PipelineLayoutsManager() = default;

	void							InitializeRHI();
	void							ReleaseRHI();

	lib::SharedPtr<PipelineLayout>	GetOrCreatePipelineLayout(const rhi::PipelineLayoutDefinition& definition);

private:

	VkDescriptorSetLayout			GetOrCreateDSLayout(const rhi::DescriptorSetDefinition& dsDef);
	VkDescriptorSetLayout			CreateDSLayout(const rhi::DescriptorSetDefinition& dsDef) const;

	lib::SharedPtr<PipelineLayout>	CreatePipelineLayout(const rhi::PipelineLayoutDefinition& definition);

	lib::HashMap<SizeType, lib::SharedPtr<PipelineLayout>>	m_cachedPipelineLayouts;
	lib::HashMap<SizeType, VkDescriptorSetLayout>			m_cachedDSLayouts;
};

} // spt::vulkan
