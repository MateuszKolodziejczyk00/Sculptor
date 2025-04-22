#pragma once

#include "PlatformMacros.h"
#include "SculptorAliases.h"

#include <string>
#include <vector>


namespace spt::platf
{

using CmdLineArgs = std::vector<std::string_view>;


class PLATFORM_API Platform
{
public:

	static Bool SwitchToThread();

	static void SleepFor(Real32 timeSeconds);

	static std::string GetExecutablePath();

	static CmdLineArgs GetCommandLineArguments();
};

} // spt::plat