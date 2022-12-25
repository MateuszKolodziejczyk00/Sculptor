#pragma once

#include "SculptorCoreTypes.h"


namespace spt::js
{

struct ThreadInfoDefinition
{
	ThreadInfoDefinition()
		: isWorker(false)
	{ }

	Bool isWorker;
};

class ThreadInfoTls
{
public:

	static void Init(const ThreadInfoDefinition& info)
	{
		isWorker = info.isWorker;
	}

	static Bool IsWorker()
	{
		return isWorker;
	}

	static void SetCurrentJob(JobInstance* job)
	{
		currentJob = job;
	}

	static JobInstance* GetCurrentJob()
	{
		return currentJob;
	}

private:

	inline static thread_local Bool isWorker = false;

	inline static thread_local JobInstance* currentJob = nullptr;
};


struct JobExecutionScope
{
public:

	explicit JobExecutionScope(JobInstance& job)
		: m_prevJob(ThreadInfoTls::GetCurrentJob())
	{
		ThreadInfoTls::SetCurrentJob(&job);
	}

	~JobExecutionScope()
	{
		ThreadInfoTls::SetCurrentJob(m_prevJob);
	}

private:

	JobInstance* m_prevJob;
};

} // spt::js