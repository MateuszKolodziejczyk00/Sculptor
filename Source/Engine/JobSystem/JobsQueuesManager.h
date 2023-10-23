#pragma once

#include "SculptorCoreTypes.h"
#include "Containers/MPMCQueue.h"
#include "WorkStealingQueue.h"
#include "JobTypes.h"
#include "Event.h"


namespace spt::js
{

class JobInstance;

using LocalQueueType	= WorkStealingQueue<JobInstance*>;
using GlobalQueueType	= lib::MPMCQueue<lib::MTHandle<JobInstance>, 64>;


class JobsQueueManagerTls
{
public:

	// Global Queue =======================================

	static Bool EnqueueGlobal(lib::MTHandle<JobInstance> job);
	static lib::MTHandle<JobInstance> DequeueGlobal(EJobPriority::Type priority);
	static lib::MTHandle<JobInstance> DequeueGlobal();

	// Local Queue ========================================

	static void InitializeLocalQueues(SizeType queuesNum);
	static void InitThreadLocalQueue(SizeType idx);

	static void EnqueueLocal(lib::MTHandle<JobInstance> job);
	static lib::MTHandle<JobInstance> DequeueLocal();

	static lib::MTHandle<JobInstance> Steal();

	// Sleep Events =======================================

	static void EnqueueSleepEvents(lib::SharedPtr<platf::Event> sleepEvent);
	static lib::SharedPtr<platf::Event> DequeueSleepEvents();

	// Active Workers =====================================

	static void IncrementActiveWorkersCount();
	static void DecrementActiveWorkersCount();

	static Int32 GetActiveWorkersCount();

	static Bool IsWorkerThread();

private:

	static GlobalQueueType s_globalQueues[EJobPriority::Num];

	thread_local static SizeType tls_localQueueIdx;

	static lib::DynamicArray<lib::UniquePtr<lib::StaticArray<LocalQueueType, EJobPriority::Num>>> s_localQueues;

	static lib::MPMCQueue<lib::SharedPtr<platf::Event>, g_maxWorkerThreadsNum> s_sleepEventsQueue;

	static std::atomic<Int32> s_activeWorkers;
};

} // spt::js