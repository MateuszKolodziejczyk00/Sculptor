#pragma once

#include "SculptorCoreTypes.h"
#include "Containers/SPSCQueue.h"
#include "Containers/MPMCQueue.h"
#include "JobTypes.h"


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

private:

	static GlobalQueueType s_globalQueues[EJobPriority::Num];

	thread_local static SizeType tls_localQueueIdx;

	static lib::DynamicArray<lib::UniquePtr<LocalQueueType>> s_localQueues;
};

} // spt::js