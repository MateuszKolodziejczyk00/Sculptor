#include "PIXBackend.h"
#include "ProfilerCore.h"

#include "windows.h"

#include "WinPixEventRuntime/pix3.h"


namespace spt::prf
{

class PIXBackend : public ProfilerImpl
{
public:

	PIXBackend() = default;

	bool IsLoaded() const { return true; }

	// Begin ProfilerImpl overrides
	virtual void BeginFrame() override;
	virtual void EndFrame() override;
	virtual void BeginEvent(const char* name) override;
	virtual void EndEvent() override;
	virtual void BeginThread(const char* name) override;
	virtual void EndThread() override;
	// End ProfilerImpl overrides
};

void PIXBackend::BeginFrame()
{
	PIXBeginEvent(0u, "FRAME");
}

void PIXBackend::EndFrame()
{
	PIXEndEvent();
}

void PIXBackend::BeginEvent(const char* name)
{
	PIXBeginEvent(0u, name);
}

void PIXBackend::EndEvent()
{
	PIXEndEvent();
}

void PIXBackend::BeginThread(const char* name)
{
	// No-op
}

void PIXBackend::EndThread()
{
	// No-op
}

ProfilerImpl* CreatePIXBackend()
{
	PIXBackend* backend = new PIXBackend();

	if (!backend->IsLoaded())
	{
		delete backend;
		backend = nullptr;
	}

	return backend;
}

} // spt::prf
