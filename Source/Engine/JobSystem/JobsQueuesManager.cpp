#include "JobsQueuesManager.h"
#include "Job.h"

namespace spt::js
{

GlobalQueueType JobsQueueManagerTls::s_globalQueues[EJobPriority::Num];
thread_local SizeType JobsQueueManagerTls::tls_localQueueIdx = idxNone<SizeType>;
lib::DynamicArray<lib::UniquePtr<LocalQueueType>> JobsQueueManagerTls::s_localQueues{};

Bool JobsQueueManagerTls::EnqueueGlobal(lib::SharedPtr<JobInstance> job)
{
	SizeType priority = static_cast<SizeType>(job->GetPriority());

	return s_globalQueues[priority].Enqueue(std::move(job));
}

lib::SharedPtr<JobInstance> JobsQueueManagerTls::DequeueGlobal(SizeType priority)
{
	return s_globalQueues[priority].Dequeue().value_or(nullptr);
}

lib::SharedPtr<JobInstance> JobsQueueManagerTls::DequeueGlobal()
{
	for (SizeType priority = 0; priority < EJobPriority::Num; ++priority)
	{
		lib::SharedPtr<JobInstance> job = DequeueGlobal(priority);
		if (job)
		{
			return job;
		}
	}

	return lib::SharedPtr<JobInstance>{};
}

void JobsQueueManagerTls::InitializeLocalQueues(SizeType queuesNum)
{
	SPT_PROFILER_FUNCTION();

	for (SizeType idx = 0; idx < queuesNum; ++idx)
	{
		s_localQueues.emplace_back(std::make_unique<LocalQueueType>());
	}
}

void JobsQueueManagerTls::InitThreadLocalQueue(SizeType idx)
{
	tls_localQueueIdx = idx;
}

void JobsQueueManagerTls::EnqueueLocal(lib::SharedPtr<JobInstance> job)
{
	SPT_CHECK(tls_localQueueIdx != idxNone<SizeType>);
	s_localQueues[tls_localQueueIdx]->Enqueue(std::move(job));
}

lib::SharedPtr<JobInstance> JobsQueueManagerTls::DequeueLocal()
{
	SPT_CHECK(tls_localQueueIdx != idxNone<SizeType>);
	return s_localQueues[tls_localQueueIdx]->Dequeue().value_or(nullptr);
}

} // spt::js