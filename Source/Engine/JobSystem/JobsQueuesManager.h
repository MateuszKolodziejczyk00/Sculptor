#pragma once

#include "SculptorCoreTypes.h"
#include "Containers/SPSCQueue.h"
#include "Containers/MPMCQueue.h"
#include "Containers/MPSCQueue.h"
#include "JobTypes.h"
#include "Event.h"


namespace spt::js
{

class JobInstance;

using LocalQueueType	= lib::SPSCQueue<lib::SharedPtr<JobInstance>>;
using GlobalQueueType	= lib::MPMCQueue<lib::SharedPtr<JobInstance>, 64>;


class JobsQueueManagerTls
{
public:

	// Global Queue =======================================

	static Bool EnqueueGlobal(lib::SharedPtr<JobInstance> job);
	static lib::SharedPtr<JobInstance> DequeueGlobal(SizeType priority);
	static lib::SharedPtr<JobInstance> DequeueGlobal();

	// Local Queue ========================================

	static void InitializeLocalQueues(SizeType queuesNum);
	static void InitThreadLocalQueue(SizeType idx);

	static void EnqueueLocal(lib::SharedPtr<JobInstance> job);
	static lib::SharedPtr<JobInstance> DequeueLocal();

	// Sleep Events =======================================

	static void EnqueueSleepEvents(lib::SharedPtr<platf::Event> sleepEvent);
	static lib::SharedPtr<platf::Event> DequeueSleepEvents();

	// Active Workers =====================================

	static void IncrementActiveWorkersCount();
	static void DecrementActiveWorkersCount();

	static Int32 GetActiveWorkersCount();

private:

	static GlobalQueueType s_globalQueues[EJobPriority::Num];

	thread_local static SizeType tls_localQueueIdx;

	static lib::DynamicArray<lib::UniquePtr<LocalQueueType>> s_localQueues;

	static lib::MPMCQueue<lib::SharedPtr<platf::Event>, g_maxWorkerThreadsNum> s_sleepEventsQueue;

	static std::atomic<Int32> s_activeWorkers;
};

} // spt::js