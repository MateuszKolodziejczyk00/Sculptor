#pragma once

#include "JobSystemMacros.h"
#include "SculptorCoreTypes.h"


namespace spt::js
{

class JobInstance;

struct ThreadInfoDefinition
{
	ThreadInfoDefinition()
		: isWorker(false)
	{ }

	Bool isWorker;
};

class JOB_SYSTEM_API ThreadInfoTls
{
public:

	static ThreadInfoTls& Get();

	void Init(const ThreadInfoDefinition& info);

	Bool IsWorker();

	void			SetCurrentJob(JobInstance* job);
	JobInstance*	GetCurrentJob();

private:

	ThreadInfoTls();

	Bool isWorker;
	JobInstance* currentJob;
};


struct JobExecutionScope
{
public:

	explicit JobExecutionScope(JobInstance& job)
		: m_prevJob(ThreadInfoTls::Get().GetCurrentJob())
	{
		ThreadInfoTls::Get().SetCurrentJob(&job);
	}

	~JobExecutionScope()
	{
		ThreadInfoTls::Get().SetCurrentJob(m_prevJob);
	}

private:

	JobInstance* m_prevJob;
};

} // spt::js