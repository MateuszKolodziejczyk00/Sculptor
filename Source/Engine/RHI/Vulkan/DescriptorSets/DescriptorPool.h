#pragma once

#include "SculptorCoreTypes.h"
#include "Vulkan/VulkanCore.h"


namespace spt::vulkan
{

class DescriptorPool
{
public:

	DescriptorPool();

	void Initialize(VkDescriptorPoolCreateFlags flags, Uint32 maxSetsNum, const VkDescriptorPoolSize* poolSizes, Uint32 poolSizesNum);
	void Release();

	Bool IsValid() const;

	VkDescriptorPool GetHandle() const;

	Bool AllocateDescriptorSets(const VkDescriptorSetLayout* layouts, Uint32 layoutsNum, lib::DynamicArray<VkDescriptorSet>& outDescriptorSets);
	void FreeDescriptorSets(const lib::DynamicArray<VkDescriptorSet>& descriptorSets);

	VkDescriptorSet AllocateDescriptorSet(VkDescriptorSetLayout layout);
	void            FreeDescriptorSet(VkDescriptorSet descriptorSet);

	void ResetPool();

private:

	VkDescriptorPool		m_poolHandle;
};

} // spt::vulkan