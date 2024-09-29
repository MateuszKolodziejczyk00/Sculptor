#include "RHIDescriptorSetManager.h"
#include "Vulkan/VulkanRHIUtils.h"
#include "Vulkan/VulkanTypes/RHIDescriptorSetLayout.h"

namespace spt::vulkan
{

RHIDescriptorSetManager& RHIDescriptorSetManager::GetInstance()
{
	static RHIDescriptorSetManager instance;
	return instance;
}

void RHIDescriptorSetManager::InitializeRHI()
{
	constexpr VkDescriptorPoolCreateFlags poolFlags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT
	                                                | VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;

	const Uint16 poolSetsNum16 = static_cast<Uint16>(m_poolSets.size());
	for (Uint16 idx = 0; idx < poolSetsNum16; ++idx)
	{
		m_poolSets[idx].poolSet.Initialize(poolFlags, idx);
	}
}

void RHIDescriptorSetManager::ReleaseRHI()
{
	std::for_each(std::begin(m_poolSets), std::end(m_poolSets),
				  [](DescriptorPoolSetData& poolSetData)
				  {
					  poolSetData.poolSet.ReleaseAllPools();
				  });

	std::for_each(std::begin(m_dynamicPoolSets), std::end(m_dynamicPoolSets),
				  [](const lib::UniquePtr<DescriptorPoolSet>& poolSet)
				  {
					  poolSet->ReleaseAllPools();
				  });
}

RHIDescriptorSet RHIDescriptorSetManager::AllocateDescriptorSet(const RHIDescriptorSetLayout& layout)
{
	DescriptorPoolSetData& poolData = LockDescriptorPoolSet();

	const RHIDescriptorSet createdDS = poolData.poolSet.AllocateDescriptorSet(layout.GetHandle());

	UnlockDescriptorPoolSet(poolData);

	return createdDS;
}

void RHIDescriptorSetManager::FreeDescriptorSet(const RHIDescriptorSet& set)
{
	DescriptorPoolSetData& poolSetData = m_poolSets[set.GetPoolSetIdx()];

	const lib::LockGuard lockGuard(poolSetData.lock);
	
	poolSetData.poolSet.FreeDescriptorSet(set.GetHandle(), set.GetPoolIdx());
}

lib::UniquePtr<DescriptorPoolSet> RHIDescriptorSetManager::AcquireDescriptorPoolSet()
{
	SPT_PROFILER_FUNCTION();

	if (!m_dynamicPoolSets.empty())
	{
		const lib::LockGuard lockGuard(m_dynamicPoolSetsLock);

		// we need to check it once again, as it might have changed when we were waiting for acquiring the lock
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
