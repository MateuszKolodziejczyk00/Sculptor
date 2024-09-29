#include "PerformanceAPIBackend.h"
#include "ProfilerCore.h"

#include "windows.h"

#include "Superluminal/PerformanceAPI_loader.h"


namespace spt::prf
{

class PerformanceAPIBackend : public ProfilerImpl
{
public:

	static constexpr const wchar_t* dllName = L"PerformanceAPI.dll";

	PerformanceAPIBackend()
	{
		m_module = PerformanceAPI_LoadFrom(dllName, &m_functions);
	}

	virtual ~PerformanceAPIBackend()
	{
		PerformanceAPI_Free(&m_module);
	}

	bool IsLoaded() const { return m_module != nullptr; }

	// Begin ProfilerImpl overrides
	virtual void BeginFrame() override;
	virtual void EndFrame() override;
	virtual void BeginEvent(const char* name) override;
	virtual void EndEvent() override;
	virtual void BeginThread(const char* name) override;
	virtual void EndThread() override;
	// End ProfilerImpl overrides

private:

	PerformanceAPI_ModuleHandle m_module;
	PerformanceAPI_Functions    m_functions;

};

void PerformanceAPIBackend::BeginFrame()
{
	m_functions.BeginEvent("FRAME", nullptr, 0u);
}

void PerformanceAPIBackend::EndFrame()
{
	m_functions.EndEvent();
}

void PerformanceAPIBackend::BeginEvent(const char* name)
{
	m_functions.BeginEvent(name, nullptr, 0u);
}

void PerformanceAPIBackend::EndEvent()
{
	m_functions.EndEvent();
}

void PerformanceAPIBackend::BeginThread(const char* name)
{
	m_functions.SetCurrentThreadName(name);
}

void PerformanceAPIBackend::EndThread()
{
	// No-op
}

ProfilerImpl* CreatePerformanceAPIBackend()
{
	PerformanceAPIBackend* backend = new PerformanceAPIBackend();

	if (!backend->IsLoaded())
	{
		delete backend;
		backend = nullptr;
	}

	return backend;
}

} // spt::prf
