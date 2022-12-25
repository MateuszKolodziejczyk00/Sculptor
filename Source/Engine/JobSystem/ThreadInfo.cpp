#include "ThreadInfo.h"


namespace spt::js
{

ThreadInfoTls& ThreadInfoTls::Get()
{
	static thread_local ThreadInfoTls instance;
	return instance;
}

void ThreadInfoTls::Init(const ThreadInfoDefinition& info)
{
	isWorker = info.isWorker;
}

Bool ThreadInfoTls::IsWorker()
{
	return isWorker;
}

void ThreadInfoTls::SetCurrentJob(JobInstance* job)
{
	currentJob = job;
}

JobInstance* ThreadInfoTls::GetCurrentJob()
{
	return currentJob;
}

ThreadInfoTls::ThreadInfoTls()
	: isWorker(false)
	, currentJob(nullptr)
{ }

} // spt::js