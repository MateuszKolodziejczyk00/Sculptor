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
	SPT_CHECK_MSG(!lib::HasAnyFlag(flags, lib::Flags(EJobFlags::Inline, EJobFlags::Local)), "Invalid flags for Launch()");
	lib::MTHandle<JobInstance> instance = JobInstanceBuilder::Build(name, std::move(callable), std::move(prerequisites), priority, flags);
	return JobBuilder::Build<TCallable>(instance);
}

template<typename TCallable>
auto Launch(const char* name, TCallable&& callable, EJobPriority::Type priority = EJobPriority::Default, EJobFlags flags = EJobFlags::Default)
{
	SPT_CHECK_MSG(!lib::HasAnyFlag(flags, lib::Flags(EJobFlags::Inline, EJobFlags::Local)), "Invalid flags for Launch()");
	lib::MTHandle<JobInstance> instance = JobInstanceBuilder::Build(name, std::move(callable), priority, flags);
	return JobBuilder::Build<TCallable>(instance);
}

template<typename TCallable>
auto LaunchInline(const char* name, TCallable&& callable)
{
	using ResultType = typename impl::JobInvokeResult<TCallable>::Type;

	JobInstance job(name);
	if constexpr (std::is_same_v<ResultType, void>)
	{
		JobInstanceBuilder::InitLocal(job, callable, EJobPriority::High, lib::Flags(EJobFlags::Inline, EJobFlags::Local));

		job.Wait();

		// This must release LAST reference to job instance (otherwise we may have dangling ptr)
		const Bool canBeDestroyed = job.Release();
		SPT_CHECK(canBeDestroyed);

		return;
	}
	else
	{
		lib::TypeStorage<ResultType> resultStorage;
		JobInstanceBuilder::InitLocal(job, callable, EJobPriority::High, lib::Flags(EJobFlags::Inline, EJobFlags::Local), &resultStorage);

		job.Wait();

		const Bool canBeDestroyed = job.Release();
		SPT_CHECK(canBeDestroyed);

		return resultStorage.Move();
	}
}

template<typename TCallable>
auto AddNested(const char* name, TCallable&& callable, EJobPriority::Type priority = EJobPriority::Default, EJobFlags flags = EJobFlags::Default)
{
	JobInstance* currentJob = ThreadInfoTls::Get().GetCurrentJob();
	SPT_CHECK(!!currentJob);
	
	lib::MTHandle<JobInstance> instance = JobInstanceBuilder::Build(name, std::move(callable), priority, flags);
	// Nested job may be already done if it's inline.
	// in this case we don't need to add it to current job
	if (!instance->IsResultReady())
	{
		currentJob->AddNested(instance);
	}
}

template<typename TRange, typename TCallable>
auto InlineParallelForEach(const char* name, TRange&& range, TCallable&& callable, EJobPriority::Type priority = EJobPriority::Default, EJobFlags iterationJobFlags = EJobFlags::Default)
{
	return LaunchInline(name,
						[name, &range, priority, iterationJobFlags, localCallable = std::forward<TCallable>(callable)]
						{
							for (auto& elem : range)
							{
								AddNested(name,
										  [&elem, &localCallable]() mutable
										  {
											  localCallable(elem);
										  },
										  priority,
										  iterationJobFlags);
							}
						});
}

template<typename TRange, typename TCallable>
auto ParallelForEach(const char* name, TRange&& range, TCallable&& callable, EJobPriority::Type priority = EJobPriority::Default, EJobFlags enclosingJobFlags = EJobFlags::Default, EJobFlags iterationJobFlags = EJobFlags::Default)
{
	return Launch(name,
				  [name, &range, priority, iterationJobFlags, localCallable = std::forward<TCallable>(callable)]
				  {
					  for (auto& elem : range)
					  {
						  AddNested(name,
									[&elem, &localCallable]() mutable
									{
										localCallable(elem);
									},
									priority,
									iterationJobFlags);
					  }
				  },
				  priority,
				  enclosingJobFlags);
}

} // spt::js