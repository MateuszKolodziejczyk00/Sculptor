#include "JobsQueuesManager.h"
#include "Job.h"

namespace spt::js
{

GlobalQueueType JobsQueueManagerTls::s_globalQueues[EJobPriority::Num];
thread_local SizeType JobsQueueManagerTls::tls_localQueueIdx = idxNone<SizeType>;
lib::DynamicArray<lib::UniquePtr<lib::StaticArray<LocalQueueType, EJobPriority::Num>>> JobsQueueManagerTls::s_localQueues{};
lib::MPMCQueue<lib::SharedPtr<platf::Event>, g_maxWorkerThreadsNum> JobsQueueManagerTls::s_sleepEventsQueue{};
std::atomic<Int32> JobsQueueManagerTls::s_activeWorkers = 0;

Bool JobsQueueManagerTls::EnqueueGlobal(lib::MTHandle<JobInstance> job)
{
	SizeType priority = static_cast<SizeType>(job->GetPriority());

	return s_globalQueues[priority].Enqueue(std::move(job));
}

lib::MTHandle<JobInstance> JobsQueueManagerTls::DequeueGlobal(EJobPriority::Type priority)
{
	return s_globalQueues[priority].Dequeue().value_or(nullptr);
}

lib::MTHandle<JobInstance> JobsQueueManagerTls::DequeueGlobal()
{
	for (SizeType priority = 0; priority < EJobPriority::Num; ++priority)
	{
		lib::MTHandle<JobInstance> job = DequeueGlobal(static_cast<EJobPriority::Type>(priority));
		if (job.IsValid())
		{
			return job;
		}
	}

	return lib::MTHandle<JobInstance>{};
}

void JobsQueueManagerTls::InitializeLocalQueues(SizeType queuesNum)
{
	SPT_PROFILER_FUNCTION();

	for (SizeType idx = 0; idx < queuesNum; ++idx)
	{
		s_localQueues.emplace_back(std::make_unique<lib::StaticArray<LocalQueueType, EJobPriority::Num>>());
	}
}

void JobsQueueManagerTls::InitThreadLocalQueue(SizeType idx)
{
	tls_localQueueIdx = idx;
}

void JobsQueueManagerTls::EnqueueLocal(lib::MTHandle<JobInstance> job)
{
	SPT_CHECK(IsWorkerThread());
	SPT_CHECK(job.IsValid());

	SizeType priority = static_cast<SizeType>(job->GetPriority());

	job->AddRef();
	s_localQueues[tls_localQueueIdx]->at(priority).Enqueue(job.Get());
}

lib::MTHandle<JobInstance> JobsQueueManagerTls::DequeueLocal()
{
	SPT_CHECK(IsWorkerThread());

	lib::MTHandle<JobInstance> job;
	for (SizeType priority = 0; !job.IsValid() && priority < EJobPriority::Num; ++priority)
	{
		job = s_localQueues[tls_localQueueIdx]->at(priority).Dequeue().value_or(nullptr);
	}

	if (job.IsValid())
	{
		job->Release();
	}

	return job;
}

lib::MTHandle<JobInstance> JobsQueueManagerTls::Steal()
{
	SPT_CHECK(IsWorkerThread());

	const SizeType potentialVictimsNum = s_localQueues.size() - 1;
	SizeType victimIdx = lib::rnd::Random<SizeType>(0u, potentialVictimsNum - 1u);
	victimIdx = (victimIdx >= tls_localQueueIdx) ? victimIdx + 1 : victimIdx;

	lib::MTHandle<JobInstance> job;
	for (SizeType priority = 0u; !job.IsValid() && priority < EJobPriority::Num; ++priority)
	{
		job = s_localQueues[victimIdx]->at(priority).Steal().value_or(nullptr);
	}

	if (job.IsValid())
	{
		job->Release();
	}
	return job;
}

void JobsQueueManagerTls::EnqueueSleepEvents(lib::SharedPtr<platf::Event> sleepEvent)
{
	SPT_PROFILER_FUNCTION();

	s_sleepEventsQueue.Enqueue(std::move(sleepEvent));
}

lib::SharedPtr<platf::Event> JobsQueueManagerTls::DequeueSleepEvents()
{
	SPT_PROFILER_FUNCTION();

	std::optional<lib::SharedPtr<platf::Event>> sleepEvent = s_sleepEventsQueue.Dequeue();
	return sleepEvent.value_or(lib::SharedPtr<platf::Event>{});
}

void JobsQueueManagerTls::IncrementActiveWorkersCount()
{
	s_activeWorkers.fetch_add(1, std::memory_order_acq_rel);
}

void JobsQueueManagerTls::DecrementActiveWorkersCount()
{
	s_activeWorkers.fetch_add(-1, std::memory_order_acq_rel);
}

Int32 JobsQueueManagerTls::GetActiveWorkersCount()
{
	return s_activeWorkers.load(std::memory_order_acquire);
}

Bool JobsQueueManagerTls::IsWorkerThread()
{
	return tls_localQueueIdx != idxNone<SizeType>;
}

} // spt::js