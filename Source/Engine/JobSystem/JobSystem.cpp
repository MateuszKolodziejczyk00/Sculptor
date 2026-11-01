#include "JobSystem.h"
#include "Scheduler.h"
#include "ThreadContext.h"

namespace spt::js
{

void JobSystem::Initialize(const JobSystemInitializationParams& initParams)
{
	tcx::InitThreadContext();

	SchedulerInitParams schedulerParams;
	schedulerParams.workerThreadsNum = initParams.workerThreadsNum;
	Scheduler::Init(schedulerParams);
}

void JobSystem::Shutdown()
{
	Scheduler::Shutdown();

	tcx::ShutdownThreadContext();
}

} // spt::js
