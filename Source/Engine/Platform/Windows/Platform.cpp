#include "Platform.h"

#include <windows.h>

namespace spt::platf
{

Bool Platform::SwitchToThread()
{
	return ::SwitchToThread();
}

} // spt::plat

