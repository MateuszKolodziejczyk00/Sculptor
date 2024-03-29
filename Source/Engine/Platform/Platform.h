#pragma once

#include "PlatformMacros.h"
#include "SculptorAliases.h"


namespace spt::platf
{

class PLATFORM_API Platform
{
public:

	static Bool SwitchToThread();

	static void SleepFor(Real32 timeSeconds);
};

} // spt::plat