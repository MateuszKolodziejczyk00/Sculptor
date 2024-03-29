#pragma once

#include "JobSystemMacros.h"
#include "SculptorCoreTypes.h"
#include "JobsQueuesManager.h"


namespace spt::js
{

class JobInstance;


struct WorkerContext
{
	WorkerContext()
		: localQueueIdx(idxNone<Uint32>)
		, shouldContinue(true)
	{ }

	WorkerContext(const WorkerContext& rhs)
		: localQueueIdx(rhs.localQueueIdx)
		, shouldContinue(rhs.shouldContinue.load(std::memory_order_acquire))
	{ }

	SizeType						localQueueIdx;
	std::atomic<Bool>				shouldContinue;
	lib::SharedPtr<platf::Event>	sleepEvent;
};


class JOB_SYSTEM_API Worker
{
public:

	static void WorkerMain(WorkerContext& inContext);
	static Bool TryExecuteJob(lib::MTHandle<JobInstance> job);

	static Bool TryActiveWait();

	explicit Worker(WorkerContext& inContext);

	void Run();

protected:

	WorkerContext& GetContext();

private:

	WorkerContext& m_workerContext;
};

} // spt::js