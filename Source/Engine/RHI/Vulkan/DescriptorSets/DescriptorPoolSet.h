#pragma once

#include "SculptorCoreTypes.h"
#include "DescriptorPool.h"
#include "Vulkan/VulkanTypes/RHIDescriptorSet.h"


namespace spt::vulkan
{

class DescriptorPoolSet
{
public:

	DescriptorPoolSet();

	void SetPoolSetIdx(Uint16 inSetIdx);

	void AllocateDescriptorSets(const VkDescriptorSetLayout* layouts, Uint32 layoutsNum, lib::DynamicArray<RHIDescriptorSet>& outDescriptorSets);
	void FreeDescriptorSets(const lib::DynamicArray<VkDescriptorSet>& dsSets, Uint16 poolIdx);
	void FreeAllDescriptorPools();

private:

	void InitializeDescriptorPool(DescriptorPool& pool);

	lib::DynamicArray<DescriptorPool>	m_descriptorPools;
	Uint16								m_thisSetIdx;
};

} // spt::vulkan