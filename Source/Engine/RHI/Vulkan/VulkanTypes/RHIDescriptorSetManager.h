#pragma once

#include "Vulkan/VulkanCore.h"
#include "SculptorCoreTypes.h"
#include "RHIDescriptorSet.h"
#include "Vulkan/DescriptorSets/DescriptorPoolSet.h"
#include "RHICore/RHIDescriptorTypes.h"


namespace spt::vulkan
{

class RHIDescriptorSetManager
{
public:

	static RHIDescriptorSetManager& GetInstance();

	void InitializeRHI();
	void ReleaseRHI();

	SPT_NODISCARD RHIDescriptorSet						AllocateDescriptorSet(const rhi::DescriptorSetLayoutID layoutID);
	SPT_NODISCARD lib::DynamicArray<RHIDescriptorSet>	AllocateDescriptorSets(const rhi::DescriptorSetLayoutID* layoutIDs, Uint32 descriptorSetsNum);
	void												FreeDescriptorSet(const RHIDescriptorSet& set);
	void												FreeDescriptorSets(const lib::DynamicArray<RHIDescriptorSet>& sets);

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
