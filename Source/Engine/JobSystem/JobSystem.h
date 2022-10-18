#pragma once

#include "JobSystemMacros.h"
#include "SculptorCoreTypes.h"


namespace spt::js
{

struct JobSystemInitializationParams
{
	explicit JobSystemInitializationParams(Uint32 inWorkerThreadsNum)
		: workerThreadsNum(inWorkerThreadsNum)
	{ }
		
	Uint32 workerThreadsNum;
};


class JOB_SYSTEM_API JobSystem
{
public:

	static void Initialize(const JobSystemInitializationParams& initParams);
};

} // spt::js