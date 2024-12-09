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

} // spt::plat

