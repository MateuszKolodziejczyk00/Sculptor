#include "RHIContext.h"



namespace spt::vulkan
{

RHIContext::RHIContext()
{ }

void RHIContext::InitializeRHI(const rhi::ContextDefinition& definition)
{
	// ...
}

void RHIContext::ReleaseRHI()
{
	// ...

	m_name.Reset();
}

Bool RHIContext::IsValid() const
{
	SPT_CHECK_NO_ENTRY();
	return false;
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
