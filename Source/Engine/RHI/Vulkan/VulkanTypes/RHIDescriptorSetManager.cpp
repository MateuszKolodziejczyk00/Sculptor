#include "RHIDescriptorSetManager.h"
#include "Vulkan/VulkanRHIUtils.h"

namespace spt::vulkan
{

void RHIDescriptorSetManager::InitializeRHI()
{
	GetInstance().InitializeRHIImpl();
}

void RHIDescriptorSetManager::ReleaseRHI()
{
	GetInstance().ReleaseRHIImpl();
}

lib::DynamicArray<RHIDescriptorSet> RHIDescriptorSetManager::AllocateDescriptorSets(const rhi::DescriptorSetLayoutID* LayoutIDs, Uint32 descriptorSetsNum)
{
	return GetInstance().AllocateDescriptorSetsImpl(LayoutIDs, descriptorSetsNum);
}

void RHIDescriptorSetManager::FreeDescriptorSets(const lib::DynamicArray<RHIDescriptorSet>& sets)
{
	return GetInstance().FreeDescriptorSetsImpl(sets);
}

RHIDescriptorSetManager::RHIDescriptorSetManager() = default;

RHIDescriptorSetManager& RHIDescriptorSetManager::GetInstance()
{
	static RHIDescriptorSetManager instance;
	return instance;
}

void RHIDescriptorSetManager::InitializeRHIImpl()
{
	const Uint16 poolSetsNum16 = static_cast<Uint16>(m_poolSets.size());
	for (Uint16 idx = 0; idx < poolSetsNum16; ++idx)
	{
		m_poolSets[idx].poolSet.SetPoolSetIdx(idx);
	}
}

void RHIDescriptorSetManager::ReleaseRHIImpl()
{
	std::for_each(std::begin(m_poolSets), std::end(m_poolSets),
				  [](DescriptorPoolSetData& poolSetData)
				  {
					  poolSetData.poolSet.FreeAllDescriptorPools();
				  });
}

lib::DynamicArray<RHIDescriptorSet> RHIDescriptorSetManager::AllocateDescriptorSetsImpl(const rhi::DescriptorSetLayoutID* LayoutIDs, Uint32 descriptorSetsNum)
{
	SPT_PROFILER_FUNCTION();

	lib::DynamicArray<RHIDescriptorSet> descriptorSets;
	descriptorSets.reserve(static_cast<SizeType>(descriptorSetsNum));

	DescriptorPoolSetData& poolData = LockDescriptorPoolSet();

	const VkDescriptorSetLayout* layoutHandles = RHIToVulkan::GetDSLayoutsPtr(LayoutIDs);
	poolData.poolSet.AllocateDescriptorSets(layoutHandles, descriptorSetsNum, descriptorSets);

	UnlockDescriptorPoolSet(poolData);

	return descriptorSets;
}

void RHIDescriptorSetManager::FreeDescriptorSetsImpl(const lib::DynamicArray<RHIDescriptorSet>& sets)
{
	SPT_PROFILER_FUNCTION();

	const auto getDSHash = [](const RHIDescriptorSet& ds)
	{
		return static_cast<Uint32>(ds.GetPoolSetIdx()) | (static_cast<Uint32>(ds.GetPoolIdx()) << 16);
	};
	
	const auto encodeSetIdx = [](Uint32 dsHash, Uint16& outPoolSetIdx, Uint16& outPoolIdx)
	{
		outPoolSetIdx = static_cast<Uint16>(dsHash & 0x0000ffff);
		outPoolIdx = static_cast<Uint16>((dsHash >> 16) & 0x0000ffff);
	};

	lib::HashMap<Uint32, lib::DynamicArray<VkDescriptorSet>> dsBatches;

	for (SizeType idx = 0; idx < sets.size(); ++idx)
	{
		dsBatches[getDSHash(sets[idx])].emplace_back(sets[idx].GetHandle());
	}

	for (auto& [dsHash, dsSets] : dsBatches)
	{
		Uint16 poolSetIdx = idxNone<Uint16>;
		Uint16 poolIdx = idxNone<Uint16>;
		encodeSetIdx(dsHash, poolSetIdx, poolIdx);

		DescriptorPoolSetData& poolSetData = m_poolSets[static_cast<SizeType>(poolSetIdx)];

		const lib::LockGuard lockGuard(poolSetData.lock);
		
		poolSetData.poolSet.FreeDescriptorSets(dsSets, poolIdx);
	}
}

RHIDescriptorSetManager::DescriptorPoolSetData& RHIDescriptorSetManager::LockDescriptorPoolSet()
{
	SPT_PROFILER_FUNCTION();

	DescriptorPoolSetData* lockedPoolsSet = nullptr;

	while (!lockedPoolsSet)
	{
		const auto foundPoolSetData = std::find_if(std::begin(m_poolSets), std::end(m_poolSets),
												   [](DescriptorPoolSetData& poolSetData)
												   {
													   return poolSetData.lock.try_lock();
												   });
		if (foundPoolSetData != std::end(m_poolSets))
		{
			lockedPoolsSet = &*foundPoolSetData;
		}
	}

	return *lockedPoolsSet;
}

void RHIDescriptorSetManager::UnlockDescriptorPoolSet(RHIDescriptorSetManager::DescriptorPoolSetData& poolSetData)
{
	poolSetData.lock.unlock();
}

} // spt::vulkan
