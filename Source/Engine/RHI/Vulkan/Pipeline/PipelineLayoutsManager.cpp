#include "PipelineLayoutsManager.h"
#include "Vulkan/VulkanRHI.h"
#include "Vulkan/VulkanRHIUtils.h"

namespace spt::vulkan
{

void PipelineLayoutsManager::InitializeRHI()
{
	// nothing here for now
}

void PipelineLayoutsManager::ReleaseRHI()
{
	SPT_PROFILER_FUNCTION();

	m_pipelinesPendingFlush.clear();
	m_cachedPipelineLayouts.clear();

	for (const auto& keyToLayout : m_cachedDSLayouts)
	{
		const VkDescriptorSetLayout handle = keyToLayout.second;
		vkDestroyDescriptorSetLayout(VulkanRHI::GetDeviceHandle(), handle, VulkanRHI::GetAllocationCallbacks());
	}

	m_cachedDSLayouts.clear();
}

lib::SharedRef<PipelineLayout> PipelineLayoutsManager::GetOrCreatePipelineLayout(const rhi::PipelineLayoutDefinition& definition)
{
	SPT_PROFILER_FUNCTION();

	const SizeType layoutDefinitionHash = lib::GetHash(definition);

	const auto cachedPipelineLayoutIt = m_cachedPipelineLayouts.find(layoutDefinitionHash);
	if (cachedPipelineLayoutIt != std::cend(m_cachedPipelineLayouts))
	{
		return lib::Ref(cachedPipelineLayoutIt->second);
	}

	{
		const lib::LockGuard lockGuard(m_pipelinesPendingFlushLock);

		auto pendingPipelineLayoutIt = m_pipelinesPendingFlush.find(layoutDefinitionHash);
		if (pendingPipelineLayoutIt == std::cend(m_pipelinesPendingFlush))
		{
			pendingPipelineLayoutIt = m_pipelinesPendingFlush.emplace(layoutDefinitionHash, CreatePipelineLayout_AssumesLocked(definition)).first;
		}

		return lib::Ref(pendingPipelineLayoutIt->second);
	}
}

void PipelineLayoutsManager::FlushPendingPipelineLayouts()
{
	SPT_PROFILER_FUNCTION();

	const lib::LockGuard lockGuard(m_pipelinesPendingFlushLock);

	std::move(std::cbegin(m_pipelinesPendingFlush), std::cend(m_pipelinesPendingFlush), std::inserter(m_cachedPipelineLayouts, std::end(m_cachedPipelineLayouts)));
	m_pipelinesPendingFlush.clear();
}

VkDescriptorSetLayout PipelineLayoutsManager::GetOrCreateDSLayout_AssumesLocked(const rhi::DescriptorSetDefinition& dsDef)
{
	SPT_PROFILER_FUNCTION();
	
	const SizeType hash = lib::GetHash(dsDef);

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
		if (bindingDef.descriptorType == rhi::EDescriptorType::None)
		{
			continue;
		}

		const VkSampler immutableSampler = bindingDef.immutableSampler.IsValid() ? bindingDef.immutableSampler.GetHandle() : VK_NULL_HANDLE;

		VkDescriptorSetLayoutBinding bindingInfo{};
		bindingInfo.binding				= bindingDef.bindingIdx;
		bindingInfo.descriptorType		= RHIToVulkan::GetDescriptorType(bindingDef.descriptorType);
		bindingInfo.descriptorCount		= bindingDef.descriptorCount;
		bindingInfo.stageFlags			= RHIToVulkan::GetShaderStages(bindingDef.shaderStages);
		bindingInfo.pImmutableSamplers = immutableSampler ? &immutableSampler : VK_NULL_HANDLE;

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
    layoutInfo.bindingCount	= static_cast<Uint32>(bindingsInfos.size());
    layoutInfo.pBindings	= bindingsInfos.data();

	VkDescriptorSetLayout layoutHandle = VK_NULL_HANDLE;
	SPT_VK_CHECK(vkCreateDescriptorSetLayout(VulkanRHI::GetDeviceHandle(), &layoutInfo, VulkanRHI::GetAllocationCallbacks(), &layoutHandle));

	SPT_CHECK(layoutHandle != VK_NULL_HANDLE);
	return layoutHandle;
}

lib::SharedRef<PipelineLayout> PipelineLayoutsManager::CreatePipelineLayout_AssumesLocked(const rhi::PipelineLayoutDefinition& definition)
{
	SPT_PROFILER_FUNCTION();

	lib::DynamicArray<VkDescriptorSetLayout> layoutHandles;
	layoutHandles.reserve(definition.descriptorSets.size());

	std::transform(std::cbegin(definition.descriptorSets), std::cend(definition.descriptorSets),
		std::back_inserter(layoutHandles),
		[this](const rhi::DescriptorSetDefinition& dsDef) -> VkDescriptorSetLayout
		{
			return GetOrCreateDSLayout_AssumesLocked(dsDef);
		});

	VulkanPipelineLayoutDefinition layoutDef;
	layoutDef.descriptorSetLayouts = std::move(layoutHandles);

	return lib::MakeShared<PipelineLayout>(layoutDef);
}

} // spt::vulkan
