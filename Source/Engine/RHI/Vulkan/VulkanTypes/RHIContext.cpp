#include "RHIContext.h"

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

void RHIContext::InitializeRHI(const rhi::ContextDefinition& definition)
{
	SPT_PROFILER_FUNCTION();

	m_id = priv::GenerateID();
}

void RHIContext::ReleaseRHI()
{
	SPT_PROFILER_FUNCTION();

	m_id = idxNone<rhi::ContextID>;

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
