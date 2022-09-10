#include "RHIDescriptorSetManager.h"

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

lib::DynamicArray<RHIDescriptorSet> RHIDescriptorSetManager::AllocateDescriptorSets(const rhi::DescriptorSetLayoutID* LayoutIDs, Uint32 DescriptorSetsNum)
{
	return GetInstance().AllocateDescriptorSetsImpl(LayoutIDs, DescriptorSetsNum);
}

RHIDescriptorSetManager::RHIDescriptorSetManager()
	: m_availablePoolSets(poolSetsNum) // by default all available
{ }

RHIDescriptorSetManager& RHIDescriptorSetManager::GetInstance()
{
	static RHIDescriptorSetManager instance;
	return instance;
}

void RHIDescriptorSetManager::InitializeRHIImpl()
{

}

void RHIDescriptorSetManager::ReleaseRHIImpl()
{
	const lib::LockGuard lockGuard(m_poolSetsLock);

	std::for_each(std::begin(m_poolSets), std::end(m_poolSets),
				  [](DescriptorPoolSetData& poolSetData)
				  {
					  SPT_CHECK(poolSetData.isLocked == false);
					  poolSetData.poolSet.FreeAllDescriptorPools();
				  });
}

lib::DynamicArray<RHIDescriptorSet> RHIDescriptorSetManager::AllocateDescriptorSetsImpl(const rhi::DescriptorSetLayoutID* LayoutIDs, Uint32 DescriptorSetsNum)
{
	SPT_PROFILER_FUNCTION();

	DescriptorPoolSetData& poolData = LockDescriptorPoolSet();

	lib::DynamicArray<RHIDescriptorSet> outDescriptorSets;
	//poolData.poolSet.AllocateDescriptorSets()

	UnlockDescriptorPoolSet(poolData);

	return outDescriptorSets;
}

RHIDescriptorSetManager::DescriptorPoolSetData& RHIDescriptorSetManager::LockDescriptorPoolSet()
{
	SPT_PROFILER_FUNCTION();
	
	lib::UnlockableLockGuard lockGuard(m_poolSetsLock);
	m_poolSetsCV.wait(lockGuard, [this]() { return m_availablePoolSets > 0; });
	
	const auto foundPoolSetData = std::find_if(std::begin(m_poolSets), std::end(m_poolSets),
											   [](const DescriptorPoolSetData& poolSetData)
											   {
												   return poolSetData.isLocked == false;
											   });

	SPT_CHECK(foundPoolSetData != std::cend(m_poolSets));

	--m_availablePoolSets;

	foundPoolSetData->isLocked = true;
	return *foundPoolSetData;
}

void RHIDescriptorSetManager::UnlockDescriptorPoolSet(RHIDescriptorSetManager::DescriptorPoolSetData& poolSetData)
{
	SPT_PROFILER_FUNCTION();
	
	{
		const lib::LockGuard lockGuard(m_poolSetsLock);

		poolSetData.isLocked = false;
		++m_availablePoolSets;
	}

	m_poolSetsCV.notify_one();
}

} // spt::vulkan
