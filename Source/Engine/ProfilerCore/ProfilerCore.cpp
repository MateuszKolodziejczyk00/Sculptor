#include "ProfilerCore.h"
#include "Backends/PerformanceAPIBackend.h"


namespace spt::prf
{

ProfilerCore& ProfilerCore::GetInstance()
{
	static ProfilerCore instance;
	return instance;
}

void ProfilerCore::Initialize()
{
	m_impl = CreatePerformanceAPIBackend();
}

void ProfilerCore::Shutdown()
{
	if (m_impl)
	{
		delete m_impl;
		m_impl = nullptr;
	}
}

ProfilerCore::ProfilerCore()
	: m_impl(nullptr)
{
}

} // spt::prf
