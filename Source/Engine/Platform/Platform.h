#pragma once

#include "PlatformMacros.h"
#include "SculptorAliases.h"

#include <string>


namespace spt::platf
{

class PLATFORM_API Platform
{
public:

	static Bool SwitchToThread();

	static void SleepFor(Real32 timeSeconds);

	static std::string GetExecutablePath();
};

} // spt::plat