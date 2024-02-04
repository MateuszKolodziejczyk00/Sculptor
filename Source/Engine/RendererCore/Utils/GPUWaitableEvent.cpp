#include "GPUWaitableEvent.h"
#include "DeviceQueues/GPUWorkload.h"
#include "Types/Semaphore.h"


namespace spt::rdr
{

GPUWaitableEvent::GPUWaitableEvent(const lib::SharedRef<GPUWorkload>& workload)
	: m_signalSemaphore(workload->GetSignaledSemaphore())
	, m_signalValue(workload->GetSignaledSemaphoreValue())
{
	SPT_CHECK(workload->IsSubmitted());
}

void GPUWaitableEvent::Wait()
{
	SPT_PROFILER_FUNCTION();

	if (m_signalSemaphore)
	{
		m_signalSemaphore->GetRHI().Wait(m_signalValue);
	}
}

} // spt::rdr
