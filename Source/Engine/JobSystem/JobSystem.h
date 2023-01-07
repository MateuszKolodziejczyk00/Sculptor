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

template<typename TCallable>
auto AddNested(const char* name, TCallable&& callable, EJobPriority::Type priority = EJobPriority::Default, EJobFlags flags = EJobFlags::Default)
{
	JobInstance* currentJob = ThreadInfoTls::Get().GetCurrentJob();
	SPT_CHECK(!!currentJob);
	
	lib::SharedRef<JobInstance> instance = JobInstanceBuilder::Build(name, std::move(callable), priority, flags);
	currentJob->AddNested(instance);
}

template<typename TRange, typename TCallable>
auto ParallelForEach(const char* name, TRange&& range, TCallable&& callable, EJobPriority::Type priority = EJobPriority::Default, EJobFlags flags = EJobFlags::Default)
{
	return Launch(name,
				  [name, &range, priority, flags, localCallable = std::forward<TCallable>(callable)]
				  {
					  lib::DynamicArray<js::Job> parallelJobs;

					  for (auto& elem : range)
					  {
						  AddNested(name,
									[&elem, &localCallable]() mutable
									{
										localCallable(elem);
									},
									priority,
									EJobFlags::Default); // we don't want to use flags from args as they should be applied only to enclosing job
					  }
				  },
				  priority,
				  flags);
}

} // spt::js