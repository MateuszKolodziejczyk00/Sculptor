#include "RHIDescriptorSetManager.h"
#include "Vulkan/VulkanRHIUtils.h"

namespace spt::vulkan
{

RHIDescriptorSetManager& RHIDescriptorSetManager::GetInstance()
{
	static RHIDescriptorSetManager instance;
	return instance;
}

void RHIDescriptorSetManager::InitializeRHI()
{
	SPT_PROFILER_FUNCTION();
	
	constexpr VkDescriptorPoolCreateFlags poolFlags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT | VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;

	const Uint16 poolSetsNum16 = static_cast<Uint16>(m_poolSets.size());
	for (Uint16 idx = 0; idx < poolSetsNum16; ++idx)
	{
		m_poolSets[idx].poolSet.Initialize(poolFlags, poolSetsNum16);
	}
}

void RHIDescriptorSetManager::ReleaseRHI()
{
	std::for_each(std::begin(m_poolSets), std::end(m_poolSets),
				  [](DescriptorPoolSetData& poolSetData)
				  {
					  poolSetData.poolSet.ReleaseAllPools();
				  });
}

RHIDescriptorSet RHIDescriptorSetManager::AllocateDescriptorSet(const rhi::DescriptorSetLayoutID layoutID)
{
	SPT_PROFILER_FUNCTION();

	const lib::DynamicArray<RHIDescriptorSet> createdDS = AllocateDescriptorSets(&layoutID, 1);
	SPT_CHECK(createdDS.size() == 1);

	return createdDS[0];
}

lib::DynamicArray<RHIDescriptorSet> RHIDescriptorSetManager::AllocateDescriptorSets(const rhi::DescriptorSetLayoutID* layoutIDs, Uint32 descriptorSetsNum)
{
	SPT_PROFILER_FUNCTION();

	lib::DynamicArray<RHIDescriptorSet> descriptorSets;
	descriptorSets.reserve(static_cast<SizeType>(descriptorSetsNum));

	DescriptorPoolSetData& poolData = LockDescriptorPoolSet();

	const VkDescriptorSetLayout* layoutHandles = RHIToVulkan::GetDSLayoutsPtr(layoutIDs);
	poolData.poolSet.AllocateDescriptorSets(layoutHandles, descriptorSetsNum, descriptorSets);

	UnlockDescriptorPoolSet(poolData);

	return descriptorSets;
}

void RHIDescriptorSetManager::FreeDescriptorSets(const lib::DynamicArray<RHIDescriptorSet>& sets)
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

lib::UniquePtr<DescriptorPoolSet> RHIDescriptorSetManager::AcquireDescriptorPoolSet()
{
	SPT_PROFILER_FUNCTION();

	if (!m_dynamicPoolSets.empty())
	{
		const lib::LockGuard lockGuard(m_dynamicPoolSetsLock);

		// we need to check it once again, as it might change when we were waiting for acquiring the lock
		if (!m_dynamicPoolSets.empty())
		{
			lib::UniquePtr<DescriptorPoolSet> poolSet = std::move(m_dynamicPoolSets.back());
			SPT_CHECK(!!poolSet);
			
			m_dynamicPoolSets.pop_back();
			
			return poolSet;
		}
	}

	// if there wasn't any free pool, create new one
	lib::UniquePtr<DescriptorPoolSet> poolSet = std::make_unique<DescriptorPoolSet>();
	poolSet->Initialize(VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT);

	return poolSet;
}

void RHIDescriptorSetManager::ReleaseDescriptorPoolSet(lib::UniquePtr<DescriptorPoolSet> poolSet)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(!!poolSet);

	poolSet->FreeAllDescriptorPools();

	const lib::LockGuard lockGuard(m_dynamicPoolSetsLock);

	m_dynamicPoolSets.emplace_back(std::move(poolSet));
}

RHIDescriptorSetManager::RHIDescriptorSetManager() = default;

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
