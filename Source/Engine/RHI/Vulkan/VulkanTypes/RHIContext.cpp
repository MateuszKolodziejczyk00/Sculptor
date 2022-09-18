#include "RHIContext.h"
#include "RHIDescriptorSetManager.h"

namespace spt::vulkan
{

namespace priv
{

static rhi::ContextID GenerateID()
{
	static std::atomic<rhi::ContextID> context = 1;
	return ++context;
}

} // priv

RHIContext::RHIContext()
	: m_id(idxNone<rhi::ContextID>)
{ }

// default destructor and move operations in .cpp file, to limit necessary includes in .h file
RHIContext::~RHIContext() = default;
RHIContext::RHIContext(RHIContext&& other) = default;
RHIContext& RHIContext::operator=(RHIContext&& rhs) = default;

void RHIContext::InitializeRHI(const rhi::ContextDefinition& definition)
{
	SPT_PROFILER_FUNCTION();

	m_id = priv::GenerateID();

	m_dynamicDescriptorsPool = RHIDescriptorSetManager::GetInstance().AcquireDescriptorPoolSet();
}

void RHIContext::ReleaseRHI()
{
	SPT_PROFILER_FUNCTION();

	m_id = idxNone<rhi::ContextID>;

	RHIDescriptorSetManager::GetInstance().ReleaseDescriptorPoolSet(std::move(m_dynamicDescriptorsPool));

	m_name.Reset();
}

Bool RHIContext::IsValid() const
{
	return m_id != idxNone<rhi::ContextID>;
}

rhi::ContextID RHIContext::GetID() const
{
	return m_id;
}

void RHIContext::SetName(const lib::HashedString& name)
{
	m_name.SetWithoutObject(name);
}

const lib::HashedString& RHIContext::GetName() const
{
	return m_name.Get();
}

} // spt::vulkan
