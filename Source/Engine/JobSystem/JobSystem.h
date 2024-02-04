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


inline Job GetCurrentJob()
{
	return Job(ThreadInfoTls::Get().GetCurrentJob());
}


struct JobDef : public JobDefinitionInternal
{
	JobDef() = default;

	JobDef& SetPriority(EJobPriority::Type inPriority)
	{
		priority = inPriority;
		return *this;
	}

	JobDef& SetFlags(EJobFlags inFlags)
	{
		flags = inFlags;
		return *this;
	}

	JobDef& ExecuteBefore(const Event& event)
	{
		executeBeforeEvent = event.GetJobInstance();
		return *this;
	}
};


template<typename TCallable, lib::CContainer TPrerequisitesRange>
auto Launch(const char* name, TCallable&& callable, TPrerequisitesRange&& prerequisites, const JobDef& def = JobDef())
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK_MSG(!lib::HasAnyFlag(def.flags, lib::Flags(EJobFlags::Inline, EJobFlags::Local)), "Invalid flags for Launch()");
	lib::MTHandle<JobInstance> instance = JobInstanceBuilder::Build(name, std::move(callable), std::move(prerequisites), def);
	return JobBuilder::Build<TCallable>(instance);
}

template<typename TCallable>
auto Launch(const char* name, TCallable&& callable, const JobDef& def = JobDef())
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK_MSG(!lib::HasAnyFlag(def.flags, lib::Flags(EJobFlags::Inline, EJobFlags::Local)), "Invalid flags for Launch()");
	lib::MTHandle<JobInstance> instance = JobInstanceBuilder::Build(name, std::move(callable), def);
	return JobBuilder::Build<TCallable>(instance);
}

template<typename TCallable>
auto LaunchInline(const char* name, TCallable&& callable)
{
	SPT_PROFILER_FUNCTION();

	using ResultType = typename impl::JobInvokeResult<TCallable>::Type;

	JobInstance job(name);
	if constexpr (std::is_same_v<ResultType, void>)
	{
		JobInstanceBuilder::InitLocal(job, callable, JobDef().SetPriority(EJobPriority::High).SetFlags(lib::Flags(EJobFlags::Inline, EJobFlags::Local)));

		job.Wait();

		// This must release LAST reference to job instance (otherwise we may have dangling ptr)
		const Bool canBeDestroyed = job.Release();
		SPT_CHECK(canBeDestroyed);

		return;
	}
	else
	{
		lib::TypeStorage<ResultType> resultStorage;
		JobInstanceBuilder::InitLocal(job, callable, JobDef().SetPriority(EJobPriority::High).SetFlags(lib::Flags(EJobFlags::Inline, EJobFlags::Local)), &resultStorage);

		job.Wait();

		const Bool canBeDestroyed = job.Release();
		SPT_CHECK(canBeDestroyed);

		return resultStorage.Move();
	}
}

template<typename TCallable>
auto AddNested(const char* name, TCallable&& callable, const JobDef& def = JobDef())
{
	SPT_PROFILER_FUNCTION();

	JobInstance* currentJob = ThreadInfoTls::Get().GetCurrentJob();
	SPT_CHECK(!!currentJob);
	
	lib::MTHandle<JobInstance> instance = JobInstanceBuilder::Build(name, std::move(callable), def);
	// Nested job may be already done if it's inline.
	// in this case we don't need to add it to current job
	if (!instance->IsResultReady())
	{
		currentJob->AddNested(instance);
	}
}

template<typename TRange, typename TCallable>
auto InlineParallelForEach(const char* name, TRange&& range, TCallable&& callable, const JobDef& iterationJobDef = JobDef())
{
	SPT_PROFILER_FUNCTION();

	return LaunchInline(name,
						[name, &range, iterationJobDef, localCallable = std::forward<TCallable>(callable)]
						{
							for (auto& elem : range)
							{
								AddNested(name,
										  [&elem, &localCallable]() mutable
										  {
											  localCallable(elem);
										  },
										  iterationJobDef);
							}
						});
}

template<typename TRange, typename TCallable>
auto ParallelForEach(const char* name, TRange&& range, TCallable&& callable, const JobDef& enclosingJobDef = JobDef(), const JobDef& iterationJobsDef = JobDef())
{
	SPT_PROFILER_FUNCTION();

	return Launch(name,
				  [name, &range, iterationJobsDef, localCallable = std::forward<TCallable>(callable)]
				  {
					  for (auto& elem : range)
					  {
						  AddNested(name,
									[&elem, &localCallable]() mutable
									{
										localCallable(elem);
									},
									iterationJobsDef);
					  }
				  },
				  enclosingJobDef);
}

inline Event CreateEvent(const char* name, const Event& executeBefore = Event())
{
	SPT_PROFILER_FUNCTION();

	const EJobFlags flags = lib::Flags(EJobFlags::Inline, EJobFlags::EventJob);
	lib::MTHandle<JobInstance> instance = JobInstanceBuilder::Build(name, [] {}, JobDef().SetFlags(flags).ExecuteBefore(executeBefore));
	return JobBuilder::BuildEvent(std::move(instance));
}


template<typename TCallable>
inline Event CreateEvent(const char* name, TCallable&& callable, const Event& executeBefore = Event())
{
	SPT_PROFILER_FUNCTION();

	const EJobFlags flags = lib::Flags(EJobFlags::Inline, EJobFlags::EventJob);
	lib::MTHandle<JobInstance> instance = JobInstanceBuilder::Build(name, callable, JobDef().SetFlags(flags).ExecuteBefore(executeBefore));
	return JobBuilder::BuildEvent(std::move(instance));
}

} // spt::js