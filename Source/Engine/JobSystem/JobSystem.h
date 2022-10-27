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
	return JobBuilder::BuildJob(std::move(callable), std::move(prerequisites), priority, flags);
}

template<typename TCallable>
auto Launch(TCallable&& callable, EJobPriority::Type priority = EJobPriority::Default, EJobFlags flags = EJobFlags::Default)
{
	return JobBuilder::BuildJob(std::move(callable), priority, flags);
}

} // spt::js