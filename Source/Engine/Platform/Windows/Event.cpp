#include "Event.h"
#include "ProfilerCore.h"

#include "windows.h"

namespace spt::platf
{

Event::Event(const wchar_t* name, Bool manualReset, Bool initialState /*= false*/)
	: m_handle(0)
{
	SPT_PROFILER_FUNCTION();

	static_assert(sizeof(IntPtr) == sizeof(HANDLE));

	m_handle = reinterpret_cast<IntPtr>(CreateEvent(NULL,			// default security attributes
													manualReset,		// manual-reset event
													initialState,	// initial state is nonsignaled
													name));			// object name
}

Event::~Event()
{
	SPT_PROFILER_FUNCTION();

	CloseHandle(reinterpret_cast<HANDLE>(m_handle));
}

void Event::Trigger()
{
	SPT_PROFILER_FUNCTION();

	SetEvent(reinterpret_cast<HANDLE>(m_handle));
}

void Event::Reset()
{
	SPT_PROFILER_FUNCTION();

	ResetEvent(reinterpret_cast<HANDLE>(m_handle));
}

void Event::Wait()
{
	SPT_PROFILER_FUNCTION();

	WaitForSingleObject(reinterpret_cast<HANDLE>(m_handle), INFINITE);
}

} // spt::platf
