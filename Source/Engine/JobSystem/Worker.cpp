#include "Worker.h"
#include "Job.h"
#include "ThreadInfo.h"

namespace spt::js
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// Private =======================================================================================

namespace priv
{

bool TryExecuteJob(const lib::SharedPtr<JobInstance>& job)
{
	if (job)
	{
		SPT_PROFILER_SCOPE("Execute Job");

		job->Execute();
		return true;
	}

	return false;
}

} // priv

//////////////////////////////////////////////////////////////////////////////////////////////////
// Worker ========================================================================================

void Worker::WorkerMain(WorkerContext& jobsQueue)
{
	SPT_PROFILER_THREAD("Worker");

	ThreadInfoDefinition threadInfo;
	threadInfo.isWorker = true;
	ThreadInfoTls::Init(threadInfo);

	JobsQueueManagerTls::InitThreadLocalQueue(jobsQueue.localQueueIdx);

	Worker worker(jobsQueue);
	worker.Run();
}

Worker::Worker(WorkerContext& inContext)
	: m_workerContext(inContext)
{ }

void Worker::Run()
{
	while (true)
	{
		if (priv::TryExecuteJob(JobsQueueManagerTls::DequeueLocal()))
		{
			continue;
		}

		// If worker doesn't have local jobs, try dequeue one from global queue
		if (priv::TryExecuteJob(JobsQueueManagerTls::DequeueGlobal()))
		{
			continue;
		}
		
		if (!GetContext().shouldContinue.load(std::memory_order_relaxed))
		{
			break;
		}
	}
}

WorkerContext& Worker::GetContext()
{
	return m_workerContext;
}

} // spt::js
