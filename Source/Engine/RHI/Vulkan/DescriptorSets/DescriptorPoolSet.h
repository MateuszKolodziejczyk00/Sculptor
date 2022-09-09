#pragma once

#include "SculptorCoreTypes.h"
#include "DescriptorPool.h"


namespace spt::vulkan
{

class DescriptorPoolSet
{
public:

	DescriptorPoolSet() = default;

	void AllocateDescriptorSets(const lib::DynamicArray<VkDescriptorSetLayout>& layouts, lib::DynamicArray<VkDescriptorSet>& outDescriptorSets);
	void FreeAllDescriptorPools();

private:

	void InitializeDescriptorPool(DescriptorPool& pool);

	lib::DynamicArray<DescriptorPool> m_descriptorPools;
};

} // spt::vulkan