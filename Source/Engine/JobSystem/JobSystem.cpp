#include "JobSystem.h"
#include "Scheduler.h"

namespace spt::js
{

void JobSystem::Initialize(const JobSystemInitializationParams& initParams)
{
	SchedulerInitParams schedulerParams;
	schedulerParams.workerThreadsNum = initParams.workerThreadsNum;
	Scheduler::Init(schedulerParams);
}

void JobSystem::Shutdown()
{
	Scheduler::Shutdown();
}

} // spt::js
