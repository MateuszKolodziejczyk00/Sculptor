#include "ProfilerCore.h"
#include "Backends/PerformanceAPIBackend.h"
#include "Backends/PIXBackend.h"


namespace spt::prf
{

ProfilerCore& ProfilerCore::GetInstance()
{
	static ProfilerCore instance;
	return instance;
}

void ProfilerCore::Initialize()
{
#if 1
	m_impl = CreatePIXBackend();
#else
	m_impl = CreatePerformanceAPIBackend();
#endif
}

void ProfilerCore::InitializeModule(ProfilerImpl* impl)
{
	m_impl = impl;
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
