#pragma once

#include "JobSystemMacros.h"
#include "SculptorCoreTypes.h"
#include "Job.h"
#include "Utility/Concepts.h"


#define SPT_GENERIC_JOB_NAME SPT_SOURCE_LOCATION


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
auto Launch(const char* name, TCallable&& callable, TPrerequisitesRange&& prerequisites, EJobPriority::Type priority = EJobPriority::Default, EJobFlags flags = EJobFlags::Default)
{
	lib::SharedRef<JobInstance> instance = JobInstanceBuilder::Build(name, std::move(callable), std::move(prerequisites), priority, flags);
	return JobBuilder::Build<TCallable>(instance);
}

template<typename TCallable>
auto Launch(const char* name, TCallable&& callable, EJobPriority::Type priority = EJobPriority::Default, EJobFlags flags = EJobFlags::Default)
{
	lib::SharedRef<JobInstance> instance = JobInstanceBuilder::Build(name, std::move(callable), priority, flags);
	return JobBuilder::Build<TCallable>(instance);
}

template<typename TRange, typename TCallable>
auto ParallelForEach(const char* name, TRange&& range, TCallable&& callable, EJobPriority::Type priority = EJobPriority::Default, EJobFlags flags = EJobFlags::Default)
{
	lib::DynamicArray<js::Job> parallelJobs;

	for (auto& elem : range)
	{
		const js::Job parallelJob = js::Launch(name,
											   [elem, callable]() mutable
											   {
												   callable(elem);
											   },
											   priority,
											   flags);

		parallelJobs.emplace_back(parallelJob);
	}

	//return js::Launch(name, [] {}, parallelJobs, EJobPriority::High);
}

} // spt::js