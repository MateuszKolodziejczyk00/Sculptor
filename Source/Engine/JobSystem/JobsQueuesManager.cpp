#include "JobsQueuesManager.h"

namespace spt::js
{

GlobalQueueType JobsQueueManagerTls::s_globalQueue{};
thread_local SizeType JobsQueueManagerTls::tls_localQueueIdx = idxNone<SizeType>;
lib::DynamicArray<lib::UniquePtr<LocalQueueType>> JobsQueueManagerTls::s_localQueues{};

void JobsQueueManagerTls::EnqueueGlobal(lib::SharedPtr<JobInstance> job)
{
	while (!s_globalQueue.Enqueue(std::move(job)));
}

lib::SharedPtr<JobInstance> JobsQueueManagerTls::DequeueGlobal()
{
	return s_globalQueue.Dequeue().value_or(nullptr);
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