#pragma once

#include "JobSystemMacros.h"
#include "SculptorCoreTypes.h"
#include "Job.h"
#include "Utility/Concepts.h"


namespace spt::js
{

struct JobSystemInitializationParams
{
	JobSystemInitializationParams()
		: workerThreadsNum(1)
	{ }

	explicit JobSystemInitializationParams(SizeType inWorkerThreadsNum)
		: workerThreadsNum(inWorkerThreadsNum)
	{ }
		
	SizeType workerThreadsNum;
};


class JOB_SYSTEM_API JobSystem
{
public:

	static void Initialize(const JobSystemInitializationParams& initParams);

	static void Shutdown();
};


template<typename TCallable, lib::CContainer TPrerequisitesRange>
auto Launch(TCallable&& callable, TPrerequisitesRange&& prerequisites, EJobPriority::Type priority = EJobPriority::Default, EJobFlags flags = EJobFlags::Default)
{
	lib::SharedRef<JobInstance> instance = JobInstanceBuilder::Build(std::move(callable), std::move(prerequisites), priority, flags);
	return JobBuilder::Build<TCallable>(instance);
}

template<typename TCallable>
auto Launch(TCallable&& callable, EJobPriority::Type priority = EJobPriority::Default, EJobFlags flags = EJobFlags::Default)
{
	lib::SharedRef<JobInstance> instance = JobInstanceBuilder::Build(std::move(callable), priority, flags);
	return JobBuilder::Build<TCallable>(instance);
}

} // spt::js