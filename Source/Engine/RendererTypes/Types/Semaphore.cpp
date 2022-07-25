#include "Semaphore.h"
#include "CurrentFrameContext.h"
#include "RendererUtils.h"

namespace spt::renderer
{

Semaphore::Semaphore(const RendererResourceName& name, const rhi::SemaphoreDefinition& definition)
{
	SPT_PROFILE_FUNCTION();

	m_rhiSemaphore.InitializeRHI(definition);
	m_rhiSemaphore.SetName(name.Get());
}

Semaphore::~Semaphore()
{
	SPT_PROFILE_FUNCTION();

	CurrentFrameContext::SubmitDeferredRelease(
		[resource = m_rhiSemaphore]() mutable
		{
			resource.ReleaseRHI();
		});
}

rhi::RHISemaphore& Semaphore::GetRHI()
{
	return m_rhiSemaphore;
}

const rhi::RHISemaphore& Semaphore::GetRHI() const
{
	return m_rhiSemaphore;
}

}
