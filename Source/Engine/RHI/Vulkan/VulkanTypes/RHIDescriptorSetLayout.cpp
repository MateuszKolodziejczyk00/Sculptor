#include "RHIDescriptorSetLayout.h"
#include "../VulkanRHIUtils.h"
#include "../VulkanRHI.h"


namespace spt::vulkan
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// RHIDescriptorSetLayoutReleaseTicket ===========================================================

void RHIDescriptorSetLayoutReleaseTicket::ExecuteReleaseRHI()
{
	if (handle.IsValid())
	{
		vkDestroyDescriptorSetLayout(VulkanRHI::GetDeviceHandle(), handle.GetValue(), VulkanRHI::GetAllocationCallbacks());
		handle.Reset();
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// RHIDescriptorSetLayout ========================================================================

RHIDescriptorSetLayout::RHIDescriptorSetLayout()
	: m_handle(VK_NULL_HANDLE)
{
}

void RHIDescriptorSetLayout::InitializeRHI(const rhi::DescriptorSetDefinition& def)
{
	SPT_PROFILER_FUNCTION();
	
	lib::DynamicArray<VkDescriptorSetLayoutBinding> bindingsInfos;
	lib::DynamicArray<VkDescriptorBindingFlags> bindingsFlags;
	bindingsInfos.reserve(def.bindings.size());
	bindingsFlags.reserve(def.bindings.size());

	const SizeType samplersNum = std::accumulate(std::cbegin(def.bindings), std::cend(def.bindings), SizeType(0),
												 [](SizeType value, const rhi::DescriptorSetBindingDefinition& bindingDef)
												 {
													 SizeType samplersInBinding = 0;
													 if(bindingDef.descriptorType == rhi::EDescriptorType::Sampler || bindingDef.descriptorType == rhi::EDescriptorType::CombinedTextureSampler)
													 {
														 SPT_CHECK_MSG(bindingDef.descriptorCount == 1, "Currently only one immutable sampler per binding is supported");
														 samplersInBinding = static_cast<SizeType>(bindingDef.descriptorCount);
													 }
													 return value + samplersInBinding;
												 });

	lib::DynamicArray<VkSampler> samplers;
	samplers.reserve(samplersNum);

	Bool hasUpdateAfterBindBindings = false;

	for (const rhi::DescriptorSetBindingDefinition& bindingDef : def.bindings)
	{
		if (bindingDef.descriptorType == rhi::EDescriptorType::None)
		{
			continue;
		}

		const Bool hasImmutableSampler = bindingDef.immutableSampler.IsValid();
		if (hasImmutableSampler)
		{
			samplers.emplace_back(bindingDef.immutableSampler.GetHandle());
		}

		VkDescriptorSetLayoutBinding bindingInfo{};
		bindingInfo.binding            = bindingDef.bindingIdx;
		bindingInfo.descriptorType     = RHIToVulkan::GetDescriptorType(bindingDef.descriptorType);
		bindingInfo.descriptorCount    = bindingDef.descriptorCount;
		bindingInfo.stageFlags         = RHIToVulkan::GetShaderStages(bindingDef.shaderStages);
		bindingInfo.pImmutableSamplers = hasImmutableSampler ? &samplers.back() : VK_NULL_HANDLE;

		const VkDescriptorBindingFlags bindingFlags = RHIToVulkan::GetBindingFlags(bindingDef.flags);

		hasUpdateAfterBindBindings |= lib::HasAnyFlag(bindingDef.flags, rhi::EDescriptorSetBindingFlags::UpdateAfterBind);

		bindingsInfos.emplace_back(bindingInfo);
		bindingsFlags.emplace_back(bindingFlags);
	}

	VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlagsInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO };
    bindingFlagsInfo.bindingCount  = static_cast<Uint32>(bindingsFlags.size());
    bindingFlagsInfo.pBindingFlags = bindingsFlags.data();

	VkDescriptorSetLayoutCreateInfo layoutInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
    layoutInfo.pNext        = &bindingFlagsInfo;
	layoutInfo.flags        = RHIToVulkan::GetDescriptorSetFlags(def.flags);
    layoutInfo.bindingCount = static_cast<Uint32>(bindingsInfos.size());
    layoutInfo.pBindings    = bindingsInfos.data();

	if (hasUpdateAfterBindBindings)
	{
		layoutInfo.flags |= VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
	}

	SPT_VK_CHECK(vkCreateDescriptorSetLayout(VulkanRHI::GetDeviceHandle(), &layoutInfo, VulkanRHI::GetAllocationCallbacks(), &m_handle));
}

void RHIDescriptorSetLayout::ReleaseRHI()
{
	RHIDescriptorSetLayoutReleaseTicket releaseTicket = DeferredReleaseRHI();
	releaseTicket.ExecuteReleaseRHI();
}

RHIDescriptorSetLayoutReleaseTicket RHIDescriptorSetLayout::DeferredReleaseRHI()
{
	SPT_CHECK(IsValid());

	RHIDescriptorSetLayoutReleaseTicket releaseTicket;
	releaseTicket.handle = m_handle;

#if SPT_RHI_DEBUG
	releaseTicket.name = GetName();
#endif SPT_RHI_DEBUG

	m_name.Reset(reinterpret_cast<Uint64>(m_handle), VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT);

	m_handle = VK_NULL_HANDLE;
	
	SPT_CHECK(!IsValid());

	return releaseTicket;
}

Bool RHIDescriptorSetLayout::IsValid() const
{
	return m_handle != VK_NULL_HANDLE;
}

void RHIDescriptorSetLayout::SetName(const lib::HashedString& name)
{
	m_name.Set(name, reinterpret_cast<Uint64>(m_handle), VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT);
}

const lib::HashedString& RHIDescriptorSetLayout::GetName() const
{
	return m_name.Get();
}

SizeType RHIDescriptorSetLayout::GetHash() const
{
	return reinterpret_cast<SizeType>(m_handle);
}

VkDescriptorSetLayout RHIDescriptorSetLayout::GetHandle() const
{
	return m_handle;
}

} // spt::vulkan
