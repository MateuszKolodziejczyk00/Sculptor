#include "Scheduler.h"
#include "Worker.h"
#include "Job.h"
#include "Event.h"

namespace spt::js
{

namespace impl
{

class SchedulerImpl
{
public:
	
	SchedulerImpl() = default;
	~SchedulerImpl();

	void InitWorkers(SizeType workersNum);

	void DestroyWorkers();

	SizeType GetWorkerThreadsNum() const;

private:

	lib::DynamicArray<WorkerContext> m_workersContexts;
	lib::DynamicArray<lib::Thread>   m_workers;
};

SchedulerImpl::~SchedulerImpl()
{
	DestroyWorkers();
}

void SchedulerImpl::InitWorkers(SizeType workersNum)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(workersNum <= g_maxWorkerThreadsNum);

	JobsQueueManagerTls::InitializeLocalQueues(workersNum);

	m_workersContexts.resize(workersNum);

	for (SizeType i = 0; i < workersNum; ++i)
	{
		m_workersContexts[i].localQueueIdx = i;
		m_workersContexts[i].sleepEvent = lib::MakeShared<platf::Event>(nullptr, false);
	}

	m_workers.reserve(workersNum);

	for (SizeType i = 0; i < workersNum; ++i)
	{
		m_workers.emplace_back(&Worker::WorkerMain, std::ref(m_workersContexts[i]));
	}
}

void SchedulerImpl::DestroyWorkers()
{
	SPT_PROFILER_FUNCTION();

	std::for_each(std::begin(m_workersContexts), std::end(m_workersContexts),
				  [](WorkerContext& context)
				  {
					  context.shouldContinue.exchange(false);
					  
					  // Wake all workers, so that they can clean all resources
					  context.sleepEvent->Trigger();
				  });

	std::for_each(std::begin(m_workers), std::end(m_workers),
				  [](lib::Thread& worker)
				  {
					  worker.Join();
				  });

	m_workersContexts.clear();
	m_workers.clear();
}

SizeType SchedulerImpl::GetWorkerThreadsNum() const
{
	return m_workers.size();
}

static SchedulerImpl& GetInstance()
{
	static SchedulerImpl instance;
	return instance;
}

} // impl

void Scheduler::Init(const SchedulerInitParams& initParams)
{
	impl::GetInstance().InitWorkers(initParams.workerThreadsNum);
}

void Scheduler::Shutdown()
{
	impl::GetInstance().DestroyWorkers();
}

void Scheduler::ScheduleJob(lib::MTHandle<JobInstance> job)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(job.IsValid());
	SPT_CHECK(!job->IsInline());

	if (impl::GetInstance().GetWorkerThreadsNum() == 0u)
	{
		// No worker threads available, execute job inline
		Worker::TryExecuteJob(job);
		return;
	}

	if (JobsQueueManagerTls::IsWorkerThread() && !job->IsForcedToGlobalQueue())
	{
		JobsQueueManagerTls::EnqueueLocal(std::move(job));
	}
	else
	{
		const EJobPriority::Type priority = job->GetPriority();

		while (!JobsQueueManagerTls::EnqueueGlobal(job))
		{
			// Execute job locally if couldn't enqueue global job
			Worker::TryExecuteJob(JobsQueueManagerTls::DequeueGlobal(priority));
		}
	}

	WakeWorker();
}

Bool Scheduler::TryExecuteScheduledJob(Bool allowLocalQueueJobs)
{
	SPT_PROFILER_FUNCTION();

	const Bool isWorkerThread = JobsQueueManagerTls::IsWorkerThread();
	return (isWorkerThread && allowLocalQueueJobs && Worker::TryExecuteJob(JobsQueueManagerTls::DequeueLocal()))
		|| Worker::TryExecuteJob(JobsQueueManagerTls::DequeueGlobal());
}

SizeType Scheduler::GetWorkerThreadsNum()
{
	return impl::GetInstance().GetWorkerThreadsNum();
}

void Scheduler::WakeWorker()
{
	lib::SharedPtr<platf::Event> sleepEndEvent = JobsQueueManagerTls::DequeueSleepEvents();

	if (sleepEndEvent)
	{
		sleepEndEvent->Trigger();
	}
}

} // spt::js
