#pragma once

#include "JobSystemMacros.h"
#include "SculptorCoreTypes.h"


namespace spt::js
{

class JobInstance;


struct SchedulerInitParams
{
	SchedulerInitParams()
		: workerThreadsNum(1)
	{ }

	SizeType workerThreadsNum;
};


class JOB_SYSTEM_API Scheduler
{
public:

	static void Init(const SchedulerInitParams& initParams);
	static void Shutdown();

	static void ScheduleJob(lib::SharedPtr<JobInstance> job);

private:

	static void WakeWorker();

	Scheduler() = default;
};

} // spt::js