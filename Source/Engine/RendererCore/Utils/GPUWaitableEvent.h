#pragma once

#include "RendererCoreMacros.h"
#include "Utility/Threading/Waitable.h"
#include "Job.h"


namespace spt::rdr
{

class GPUWorkload;
class Semaphore;


class RENDERER_CORE_API GPUWaitableEvent : public lib::Waitable
{
public:

	explicit GPUWaitableEvent(const lib::SharedRef<GPUWorkload>& workload);

	// Begin Waitable implementation
	virtual void Wait() override;
	// End Waitable implementation

private:

	lib::SharedPtr<rdr::Semaphore> m_signalSemaphore;
	Uint64                         m_signalValue;
};

} // spt::rdr