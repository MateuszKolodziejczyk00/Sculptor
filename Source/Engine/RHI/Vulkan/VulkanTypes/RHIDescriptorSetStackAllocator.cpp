#include "RHIDescriptorSetStackAllocator.h"
#include "Vulkan/DescriptorSets/RHIDescriptorSetManager.h"


namespace spt::vulkan
{

RHIDescriptorSetStackAllocator::RHIDescriptorSetStackAllocator()
	: m_poolSet(nullptr)
{
}

void RHIDescriptorSetStackAllocator::InitializeRHI()
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(!IsValid());

	lib::UniquePtr<DescriptorPoolSet> poolSet = RHIDescriptorSetManager::GetInstance().AcquireDescriptorPoolSet();
	m_poolSet = poolSet.release();
}

void RHIDescriptorSetStackAllocator::ReleaseRHI()
{
	SPT_PROFILER_FUNCTION();
	
	SPT_CHECK(IsValid());

	RHIDescriptorSetManager::GetInstance().ReleaseDescriptorPoolSet(lib::UniquePtr<DescriptorPoolSet>(m_poolSet));
	m_poolSet = nullptr;
}

Bool RHIDescriptorSetStackAllocator::IsValid() const
{
	return !!m_poolSet;
}

RHIDescriptorSet RHIDescriptorSetStackAllocator::AllocateDescriptorSet(const RHIDescriptorSetLayout& layout)
{
	SPT_CHECK(IsValid());

	return m_poolSet->AllocateDescriptorSet(layout.GetHandle());
}

void RHIDescriptorSetStackAllocator::SetName(const lib::HashedString& name)
{
	m_name.SetWithoutObject(name);
}

const lib::HashedString& RHIDescriptorSetStackAllocator::GetName() const
{
	return m_name.Get();
}

} // spt::vulkan
