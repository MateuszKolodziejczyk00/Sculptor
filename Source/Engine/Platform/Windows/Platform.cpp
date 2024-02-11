#include "Platform.h"

#include <windows.h>

namespace spt::platf
{

Bool Platform::SwitchToThread()
{
	return ::SwitchToThread();
}

void Platform::SleepFor(Real32 timeSeconds)
{
	::Sleep(static_cast<DWORD>(timeSeconds * 1000));
}

} // spt::plat

