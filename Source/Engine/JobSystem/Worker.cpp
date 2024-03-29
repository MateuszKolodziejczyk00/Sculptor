#include "Worker.h"
#include "Job.h"
#include "ThreadInfo.h"

namespace spt::js
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// Utils =========================================================================================

class ActiveWorkerGuard
{
public:

	explicit ActiveWorkerGuard(Bool isActive)
		: m_isActive(isActive)
	{ }

	~ActiveWorkerGuard()
	{
		if (m_isActive)
		{
			JobsQueueManagerTls::DecrementActiveWorkersCount();
		}
	}

	void OnActivate()
	{
		if (!m_isActive)
		{
			m_isActive = true;
			JobsQueueManagerTls::IncrementActiveWorkersCount();
		}
	}

	void OnDeativate()
	{
		if (m_isActive)
		{
			m_isActive = false;
			JobsQueueManagerTls::DecrementActiveWorkersCount();
		}
	}

private:

	Bool m_isActive;
};

//////////////////////////////////////////////////////////////////////////////////////////////////
// Worker ========================================================================================

void Worker::WorkerMain(WorkerContext& jobsQueue)
{
	SPT_PROFILER_THREAD("Worker");

	ThreadInfoDefinition threadInfo;
	threadInfo.isWorker = true;
	ThreadInfoTls::Get().Init(threadInfo);

	JobsQueueManagerTls::InitThreadLocalQueue(jobsQueue.localQueueIdx);

	Worker worker(jobsQueue);
	worker.Run();
}

Bool Worker::TryExecuteJob(lib::MTHandle<JobInstance> job)
{
	if (job.IsValid())
	{
		SPT_PROFILER_SCOPE("Execute Job");

		JobInstance* jobPtr = job.Get();
		// make sure we release all references before finishing local job
		if (jobPtr->IsLocal())
		{
			job.Reset();
		}

		jobPtr->TryExecute();
		return true;
	}

	return false;
}

Bool Worker::TryActiveWait()
{
	const SizeType workersNum = Scheduler::GetWorkerThreadsNum();

	return TryExecuteJob(JobsQueueManagerTls::DequeueLocal())
		|| TryExecuteJob(JobsQueueManagerTls::DequeueGlobal())
		|| TryExecuteJob(JobsQueueManagerTls::Steal(static_cast<Uint32>(workersNum)));
}

Worker::Worker(WorkerContext& inContext)
	: m_workerContext(inContext)
{ }

void Worker::Run()
{
	constexpr SizeType idleLoopsRequiredToSleep = 192u;
	SizeType currentIdleLoopsNum = 0;

	ActiveWorkerGuard activeWorkerGuard(false);

	while (true)
	{
		if (	TryExecuteJob(JobsQueueManagerTls::DequeueLocal())
			||	TryExecuteJob(JobsQueueManagerTls::DequeueGlobal())
			||	TryExecuteJob(JobsQueueManagerTls::Steal()))
		{
			currentIdleLoopsNum = 0;
			continue;
		}
		
		if (!GetContext().shouldContinue.load(std::memory_order_relaxed))
		{
			break;
		}

		++currentIdleLoopsNum;

		if (currentIdleLoopsNum == idleLoopsRequiredToSleep)
		{
			activeWorkerGuard.OnDeativate();

			currentIdleLoopsNum = 0;
			JobsQueueManagerTls::EnqueueSleepEvents(GetContext().sleepEvent);
			GetContext().sleepEvent->Wait();

			activeWorkerGuard.OnActivate();
		}
	}
}

WorkerContext& Worker::GetContext()
{
	return m_workerContext;
}

} // spt::js
