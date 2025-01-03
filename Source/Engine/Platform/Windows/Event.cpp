#include "Event.h"
#include "ProfilerCore.h"

#include "windows.h"
#include "SculptorLib/Assertions/Assertions.h"
#include "SculptorLib/Utility/UtilityMacros.h"

namespace spt::platf
{

Event::Event(const wchar_t* name, Bool manualReset, Bool initialState /*= false*/)
	: m_handle(0)
{
	SPT_PROFILER_FUNCTION();

	static_assert(sizeof(IntPtr) == sizeof(HANDLE));

	m_handle = reinterpret_cast<IntPtr>(CreateEvent(NULL, manualReset, initialState, name));
}

Event::Event(Bool manualReset, Bool initialState /*= false*/)
	: m_handle(0)
{
	SPT_PROFILER_FUNCTION();

	static_assert(sizeof(IntPtr) == sizeof(HANDLE));

	m_handle = reinterpret_cast<IntPtr>(CreateEvent(NULL, manualReset, initialState, NULL));
}

Event::~Event()
{
	SPT_PROFILER_FUNCTION();

	CloseHandle(reinterpret_cast<HANDLE>(m_handle));
}

void Event::Trigger()
{
	SetEvent(reinterpret_cast<HANDLE>(m_handle));
}

void Event::Reset()
{
	ResetEvent(reinterpret_cast<HANDLE>(m_handle));
}

Bool Event::IsTriggered() const
{
	return WaitForSingleObject(reinterpret_cast<HANDLE>(m_handle), 0) != WAIT_TIMEOUT;
}

void Event::Wait()
{
	SPT_PROFILER_FUNCTION();

	WaitForSingleObject(reinterpret_cast<HANDLE>(m_handle), INFINITE);
}

Bool Event::Wait(Uint64 milliseconds)
{
	SPT_PROFILER_FUNCTION();

	const DWORD result = WaitForSingleObject(reinterpret_cast<HANDLE>(m_handle), static_cast<DWORD>(milliseconds));
	return result == WAIT_OBJECT_0;
}

} // spt::platf
