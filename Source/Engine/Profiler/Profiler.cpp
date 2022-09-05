#include "Profiler.h"
#include "Paths.h"
#include <ctime>
#include <iomanip>

namespace spt::prf
{

namespace utils
{

static lib::String GetCaptureFileNameWithoutExtension()
{
    const std::time_t currentTime = std::time(nullptr);

	std::tm localTime{};
	localtime_s(&localTime, &currentTime);

    std::ostringstream oss;
    oss << std::put_time(&localTime, "%d%m%Y_%H%M%S");
    const lib::String timeString = oss.str();

	return timeString;
}

} // utils

namespace impl
{

class NullProfilerImpl
{
public:

	static void StartCapture() {}

	static void StopCapture() {}

	static Bool SaveCapture() { return false; }
};

#if WITH_OPTICK

class OptickProfilerImpl
{
public:

	static void StartCapture()
	{
		OPTICK_START_CAPTURE();
	}

	static void StopCapture()
	{
		OPTICK_STOP_CAPTURE();
	}

	static Bool SaveCapture()
	{
		const lib::String CaptureFileName = utils::GetCaptureFileNameWithoutExtension() + ".opt";
		const lib::String CaptureFilePath = engn::Paths::Combine(engn::Paths::GetTracesPath(), CaptureFileName);

		OPTICK_SAVE_CAPTURE(CaptureFilePath.c_str());

		return true;
	}
};

#endif // WITH_OPTICK

#if WITH_OPTICK

using ProfilerImpl = OptickProfilerImpl;

#else

using ProfilerImpl = NullProfilerImpl;

#endif // WITH_OPTICK

} // impl

namespace priv
{

// probably doesn't need to be threadsafe
static Bool g_startedCapture = false;

} // priv

void Profiler::StartCapture()
{
	impl::ProfilerImpl::StartCapture();
	priv::g_startedCapture = true;
}

void Profiler::StopCapture()
{
	impl::ProfilerImpl::StopCapture();
	priv::g_startedCapture = false;
}

Bool Profiler::StartedCapture()
{
	return priv::g_startedCapture;
}

Bool Profiler::SaveCapture()
{
	return impl::ProfilerImpl::SaveCapture();
}

} // spt::prf
