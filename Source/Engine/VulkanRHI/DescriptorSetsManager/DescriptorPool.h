#pragma once

#include "SculptorCoreTypes.h"
#include "Vulkan.h"


namespace spt::vulkan
{

class DescriptorPool
{
public:

	DescriptorPool();

	void					Initialize(VkDescriptorPoolCreateFlags flags, Uint32 maxSetsNum, const lib::DynamicArray<VkDescriptorPoolSize>& poolSizes);
	void					Release();

	Bool					IsValid() const;

	VkDescriptorPool		GetHandle() const;

	Bool					AllocateDescriptorSets(const lib::DynamicArray<VkDescriptorSetLayout>& layouts, lib::DynamicArray<VkDescriptorSet>& outDescriptorSets);

	void					ResetPool();

private:

	VkDescriptorPool		m_poolHandle;
};

}