#pragma once

#include "SculptorCoreTypes.h"
#include "JobsQueuesManager.h"


namespace spt::js
{

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

	SizeType			localQueueIdx;
	std::atomic<Bool>	shouldContinue;
};


class Worker
{
public:

	static void WorkerMain(WorkerContext& inContext);

	Worker(WorkerContext& inContext);

	void Run();

protected:

	WorkerContext& GetContext();

private:

	WorkerContext& m_workerContext;
};

} // spt::js