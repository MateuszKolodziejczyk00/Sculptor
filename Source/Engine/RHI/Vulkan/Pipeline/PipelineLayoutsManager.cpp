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
	SPT_PROFILER_FUNCTION();

	m_cachedPipelineLayouts.clear();

	for (const auto& keyToLayout : m_cachedDSLayouts)
	{
		const VkDescriptorSetLayout handle = keyToLayout.second;
		vkDestroyDescriptorSetLayout(VulkanRHI::GetDeviceHandle(), handle, VulkanRHI::GetAllocationCallbacks());
	}

	m_cachedDSLayouts.clear();
}

lib::SharedPtr<PipelineLayout> PipelineLayoutsManager::GetOrCreatePipelineLayout(const rhi::PipelineLayoutDefinition& definition)
{
	SPT_PROFILER_FUNCTION();

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
	SPT_PROFILER_FUNCTION();
	
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
	SPT_PROFILER_FUNCTION();

	lib::DynamicArray<VkDescriptorSetLayoutBinding> bindingsInfos;
	lib::DynamicArray<VkDescriptorBindingFlags> bindingsFlags;
	bindingsInfos.reserve(dsDef.bindings.size());
	bindingsFlags.reserve(dsDef.bindings.size());

	for (const rhi::DescriptorSetBindingDefinition& bindingDef : dsDef.bindings)
	{
		VkDescriptorSetLayoutBinding bindingInfo{};
		bindingInfo.binding				= bindingDef.bindingIdx;
		bindingInfo.descriptorType		= RHIToVulkan::GetDescriptorType(bindingDef.descriptorType);
		bindingInfo.descriptorCount		= bindingDef.descriptorCount;
		bindingInfo.stageFlags			= RHIToVulkan::GetStageFlagsLegacy(bindingDef.shaderStages);
		bindingInfo.pImmutableSamplers	= nullptr; // Currently not supported

		const VkDescriptorBindingFlags bindingFlags = RHIToVulkan::GetBindingFlags(bindingDef.flags);

		bindingsInfos.emplace_back(bindingInfo);
		bindingsFlags.emplace_back(bindingFlags);
	}

	VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlagsInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO };
    bindingFlagsInfo.bindingCount	= static_cast<Uint32>(bindingsFlags.size());
    bindingFlagsInfo.pBindingFlags	= bindingsFlags.data();

	VkDescriptorSetLayoutCreateInfo layoutInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
    layoutInfo.pNext		= &bindingFlagsInfo;
	layoutInfo.flags		= RHIToVulkan::GetDescriptorSetFlags(dsDef.flags);
    layoutInfo.bindingCount	= static_cast<Uint32>(dsDef.bindings.size());
    layoutInfo.pBindings	= bindingsInfos.data();

	VkDescriptorSetLayout layoutHandle = VK_NULL_HANDLE;
	SPT_VK_CHECK(vkCreateDescriptorSetLayout(VulkanRHI::GetDeviceHandle(), &layoutInfo, VulkanRHI::GetAllocationCallbacks(), &layoutHandle));

	SPT_CHECK(layoutHandle != VK_NULL_HANDLE);
	return layoutHandle;
}

lib::SharedPtr<PipelineLayout> PipelineLayoutsManager::CreatePipelineLayout(const rhi::PipelineLayoutDefinition& definition)
{
	SPT_PROFILER_FUNCTION();

	lib::DynamicArray<VkDescriptorSetLayout> layoutHandles;
	layoutHandles.reserve(definition.descriptorSets.size());

	std::transform(std::cbegin(definition.descriptorSets), std::cend(definition.descriptorSets),
		std::back_inserter(layoutHandles),
		[this](const rhi::DescriptorSetDefinition& dsDef) -> VkDescriptorSetLayout
		{
			return GetOrCreateDSLayout(dsDef);
		});

	VulkanPipelineLayoutDefinition layoutDef;
	layoutDef.descriptorSetLayouts = std::move(layoutHandles);

	return std::make_shared<PipelineLayout>(layoutDef);
}

} // spt::vulkan
