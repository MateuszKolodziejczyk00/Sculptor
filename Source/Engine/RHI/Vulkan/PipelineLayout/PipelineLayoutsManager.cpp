#include "PipelineLayoutsManager.h"
#include "Vulkan/VulkanRHI.h"
#include "Vulkan/RHIToVulkanCommon.h"

namespace spt::vulkan
{

void PipelineLayoutsManager::InitializeRHI()
{
	// nothing here for now
}

void PipelineLayoutsManager::ReleaseRHI()
{
	SPT_PROFILE_FUNCTION();

	for (const auto& keyToLayout : m_cachedDSLayouts)
	{
		const VkDescriptorSetLayout handle = keyToLayout.second;
		vkDestroyDescriptorSetLayout(VulkanRHI::GetDeviceHandle(), handle, VulkanRHI::GetAllocationCallbacks());
	}

	for (const auto& keyToLayout : m_cachedPipelineLayouts)
	{
		const VkPipelineLayout handle = keyToLayout.second;
		vkDestroyPipelineLayout(VulkanRHI::GetDeviceHandle(), handle, VulkanRHI::GetAllocationCallbacks());
	}

	m_cachedDSLayouts.clear();
	m_cachedPipelineLayouts.clear();
}

VkPipelineLayout PipelineLayoutsManager::GetOrCreatePipelineLayout(const rhi::PipelineLayoutDefinition& definition)
{
	SPT_PROFILE_FUNCTION();

	const std::hash<rhi::PipelineLayoutDefinition> hasher;
	const SizeType layoutDefinitionHash = hasher(definition);

	auto pipelineLayoutIt = m_cachedPipelineLayouts.find(layoutDefinitionHash);
	if (pipelineLayoutIt == std::cend(m_cachedPipelineLayouts))
	{
		pipelineLayoutIt = m_cachedPipelineLayouts.emplace(layoutDefinitionHash, CreatePipelineLayout(definition)).first;
	}

	return pipelineLayoutIt->second;
}

VkDescriptorSetLayout PipelineLayoutsManager::GetOrCreateDSLayout(const rhi::DescriptorSetDefinition& dsDef)
{
	SPT_PROFILE_FUNCTION();
	
	const std::hash<rhi::DescriptorSetDefinition> hasher;
	const SizeType hash = hasher(dsDef);

	auto layoutIt = m_cachedDSLayouts.find(hash);
	if (layoutIt == std::cend(m_cachedDSLayouts))
	{
		layoutIt = m_cachedDSLayouts.emplace(hash, CreateDSLayout(dsDef)).first;
	}

	return layoutIt->second;
}

VkDescriptorSetLayout PipelineLayoutsManager::CreateDSLayout(const rhi::DescriptorSetDefinition& dsDef) const
{
	SPT_PROFILE_FUNCTION();

	lib::DynamicArray<VkDescriptorSetLayoutBinding> bindingsInfos;
	lib::DynamicArray<VkDescriptorBindingFlags> bindingsFlags;
	bindingsInfos.reserve(dsDef.m_bindings.size());
	bindingsFlags.reserve(dsDef.m_bindings.size());

	for (const rhi::DescriptorSetBindingDefinition& bindingDef : dsDef.m_bindings)
	{
		VkDescriptorSetLayoutBinding bindingInfo{};
		bindingInfo.binding				= bindingDef.m_bindingIdx;
		bindingInfo.descriptorType		= RHIToVulkan::GetDescriptorType(bindingDef.m_descriptorType);
		bindingInfo.descriptorCount		= bindingDef.m_descriptorCount;
		bindingInfo.stageFlags			= RHIToVulkan::GetStageFlagsLegacy(bindingDef.m_pipelineStages);
		bindingInfo.pImmutableSamplers	= nullptr; // Currently not supported

		const VkDescriptorBindingFlags bindingFlags = RHIToVulkan::GetBindingFlags(bindingDef.m_flags);

		bindingsInfos.emplace_back(bindingInfo);
		bindingsFlags.emplace_back(bindingFlags);
	}

	VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlagsInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO };
    bindingFlagsInfo.bindingCount	= static_cast<Uint32>(bindingsFlags.size());
    bindingFlagsInfo.pBindingFlags	= bindingsFlags.data();

	VkDescriptorSetLayoutCreateInfo layoutInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
    layoutInfo.pNext		= &bindingFlagsInfo;
	layoutInfo.flags		= RHIToVulkan::GetDescriptorSetFlags(dsDef.m_flags);
    layoutInfo.bindingCount	= static_cast<Uint32>(dsDef.m_bindings.size());
    layoutInfo.pBindings	= bindingsInfos.data();

	VkDescriptorSetLayout layoutHandle = VK_NULL_HANDLE;
	SPT_VK_CHECK(vkCreateDescriptorSetLayout(VulkanRHI::GetDeviceHandle(), &layoutInfo, VulkanRHI::GetAllocationCallbacks(), &layoutHandle));

	SPT_CHECK(layoutHandle != VK_NULL_HANDLE);
	return layoutHandle;
}

VkPipelineLayout PipelineLayoutsManager::CreatePipelineLayout(const rhi::PipelineLayoutDefinition& definition)
{
	SPT_PROFILE_FUNCTION();

	lib::DynamicArray<VkDescriptorSetLayout> layoutHandles;
	layoutHandles.reserve(definition.m_descriptorSets.size());

	std::transform(std::cbegin(definition.m_descriptorSets), std::cend(definition.m_descriptorSets),
		std::back_inserter(layoutHandles),
		[this](const rhi::DescriptorSetDefinition& dsDef) -> VkDescriptorSetLayout
		{
			return GetOrCreateDSLayout(dsDef);
		});

	VkPipelineLayoutCreateInfo pipelineLayoutInfo{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    pipelineLayoutInfo.setLayoutCount			= static_cast<Uint32>(layoutHandles.size());
    pipelineLayoutInfo.pSetLayouts				= layoutHandles.data();
    pipelineLayoutInfo.pushConstantRangeCount	= 0;
	pipelineLayoutInfo.pPushConstantRanges		= nullptr;

	VkPipelineLayout pipelineLayoutHandle = VK_NULL_HANDLE;
	SPT_VK_CHECK(vkCreatePipelineLayout(VulkanRHI::GetDeviceHandle(), &pipelineLayoutInfo, VulkanRHI::GetAllocationCallbacks(), &pipelineLayoutHandle));

	SPT_CHECK(pipelineLayoutHandle != VK_NULL_HANDLE);
	return pipelineLayoutHandle;
}

} // spt::vulkan
