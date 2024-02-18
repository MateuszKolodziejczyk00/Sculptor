#pragma once

#include "Vulkan/VulkanCore.h"
#include "SculptorCoreTypes.h"
#include "Vulkan/DescriptorSets/DescriptorPoolSet.h"
#include "RHICore/RHIDescriptorTypes.h"
#include "Vulkan/VulkanTypes/RHIDescriptorSet.h"


namespace spt::vulkan
{

class RHIDescriptorSetLayout;


class RHIDescriptorSetManager
{
public:

	static RHIDescriptorSetManager& GetInstance();

	void InitializeRHI();
	void ReleaseRHI();

	SPT_NODISCARD RHIDescriptorSet AllocateDescriptorSet(const RHIDescriptorSetLayout& layout);
	void                           FreeDescriptorSet(const RHIDescriptorSet& set);

	// Dynamic descriptor pools ===================================

	SPT_NODISCARD lib::UniquePtr<DescriptorPoolSet>	AcquireDescriptorPoolSet();
	void											ReleaseDescriptorPoolSet(lib::UniquePtr<DescriptorPoolSet> poolSet);

private:

	struct DescriptorPoolSetData
	{
		DescriptorPoolSetData() = default;

		DescriptorPoolSet	poolSet;
		lib::Lock			lock;
	};
	
	RHIDescriptorSetManager();

	SPT_NODISCARD DescriptorPoolSetData&	LockDescriptorPoolSet();
	void									UnlockDescriptorPoolSet(DescriptorPoolSetData& poolSetData);

	static constexpr SizeType poolSetsNum = 4;

	lib::StaticArray<DescriptorPoolSetData, poolSetsNum>	m_poolSets;

	lib::DynamicArray<lib::UniquePtr<DescriptorPoolSet>>	m_dynamicPoolSets;
	lib::Lock m_dynamicPoolSetsLock;
};

} // spt::vulkan
