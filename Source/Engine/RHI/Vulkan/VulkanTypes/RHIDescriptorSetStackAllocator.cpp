#include "RHIDescriptorSetStackAllocator.h"
#include "Vulkan/DescriptorSets/RHIDescriptorSetManager.h"


namespace spt::vulkan
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// RHIDescriptorSetStackAllocatorReleaseTicket ===================================================

void RHIDescriptorSetStackAllocatorReleaseTicket::ExecuteReleaseRHI()
{
	if (handle.IsValid())
	{
		RHIDescriptorSetManager::GetInstance().ReleaseDescriptorPoolSet(lib::UniquePtr<DescriptorPoolSet>(handle.GetValue()));
		handle.Reset();
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// RHIDescriptorSetStackAllocator ================================================================

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
	
	SPT_CHECK(IsValid());
}

void RHIDescriptorSetStackAllocator::ReleaseRHI()
{
	RHIDescriptorSetStackAllocatorReleaseTicket releaseTicket = DeferredReleaseRHI();
	releaseTicket.ExecuteReleaseRHI();
}

RHIDescriptorSetStackAllocatorReleaseTicket RHIDescriptorSetStackAllocator::DeferredReleaseRHI()
{
	SPT_CHECK(IsValid());

	RHIDescriptorSetStackAllocatorReleaseTicket releaseTicket;
	releaseTicket.handle = m_poolSet;

	m_poolSet = nullptr;

	SPT_CHECK(!IsValid());

	return releaseTicket;
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
