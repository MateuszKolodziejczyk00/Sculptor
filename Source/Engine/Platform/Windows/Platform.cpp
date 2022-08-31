#include "Platform.h"

#include <windows.h>

namespace spt::plat
{

Bool Platform::SwitchToThread()
{
	return ::SwitchToThread();
}

} // spt::plat

