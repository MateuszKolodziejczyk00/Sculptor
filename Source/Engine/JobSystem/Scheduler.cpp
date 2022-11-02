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

private:

	lib::DynamicArray<WorkerContext>				m_workersContexts;
	lib::DynamicArray<std::thread>					m_workers;
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
		m_workersContexts[i].sleepEvent = lib::MakeShared<platf::Event>(L"WorkerSleepEvent", false);
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
					  context.shouldContinue.exchange(false, std::memory_order_release);
					  
					  // Wake all workers, so that they can clean all resources
					  context.sleepEvent->Trigger();
				  });

	std::for_each(std::begin(m_workers), std::end(m_workers),
				  [](std::thread& worker)
				  {
					  worker.join();
				  });

	m_workersContexts.clear();
	m_workers.clear();
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

void Scheduler::ScheduleJob(lib::SharedPtr<JobInstance> job)
{
	SPT_PROFILER_FUNCTION();

	const SizeType priority = job->GetPriority();

	while (!JobsQueueManagerTls::EnqueueGlobal(job))
	{
		// Execute job locally if couldn't enqueue global job
		Worker::TryExecuteJob(JobsQueueManagerTls::DequeueGlobal(priority));
	}

	WakeWorker();
}

void Scheduler::WakeWorker()
{
	SPT_PROFILER_FUNCTION();

	lib::SharedPtr<platf::Event> sleepEndEvent = JobsQueueManagerTls::DequeueSleepEvents();

	if (sleepEndEvent)
	{
		sleepEndEvent->Trigger();
	}
}

} // spt::js
