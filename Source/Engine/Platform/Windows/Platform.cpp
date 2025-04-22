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

std::string Platform::GetExecutablePath()
{
	static constexpr SizeType maxPathLength = 255u;
	std::string path;
	path.resize(maxPathLength);

	const DWORD length = ::GetModuleFileNameA(nullptr, path.data(), static_cast<DWORD>(path.size()));
	if (length == 0)
	{
		return {};
	}

	path.resize(static_cast<SizeType>(length));
	return path;
}

CmdLineArgs Platform::GetCommandLineArguments()
{
	const char* cmdLine = ::GetCommandLineA();
	const char* current = cmdLine;
	CmdLineArgs args;
	while (*current != '\0')
	{
		while (*current == ' ')
		{
			current++;
		}
		if (*current == '\0')
		{
			break;
		}
		const char* start = current;
		while (*current != ' ' && *current != '\0')
		{
			current++;
		}
		args.emplace_back(start, static_cast<SizeType>(current - start));
	}
	return args;
}

} // spt::plat

