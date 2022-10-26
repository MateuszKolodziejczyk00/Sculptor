#include "Scheduler.h"
#include "Worker.h"

namespace spt::js
{

namespace impl
{

class SchedulerImpl
{
public:
	
	SchedulerImpl() = default;

	void InitWorkers(SizeType workersNum);

	void DestroyWorkers();

private:

	lib::DynamicArray<WorkerContext>	m_workersContexts;
	lib::DynamicArray<std::thread>		m_workers;
};

void SchedulerImpl::InitWorkers(SizeType workersNum)
{
	SPT_PROFILER_FUNCTION();

	JobsQueueManagerTls::InitializeLocalQueues(workersNum);

	m_workersContexts.resize(workersNum);

	for (SizeType i = 0; i < workersNum; ++i)
	{
		m_workersContexts[i].localQueueIdx = i;
	}

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

	JobsQueueManagerTls::EnqueueGlobal(std::move(job));
}

} // spt::js
