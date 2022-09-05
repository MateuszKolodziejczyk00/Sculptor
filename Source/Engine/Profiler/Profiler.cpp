#include "Profiler.h"
#include "Paths.h"
#include "FileSystem/File.h"

#include <ctime>
#include <iomanip>

namespace spt::prf
{

namespace priv
{

// probably doesn't need to be threadsafe
static Bool				g_startedCapture = false;
static std::ofstream	g_currentCaptureStream;

} // priv

namespace utils
{

static lib::String GetCaptureFilePathWithoutExtension()
{
    const std::time_t currentTime = std::time(nullptr);

	std::tm localTime{};
	localtime_s(&localTime, &currentTime);

    std::ostringstream oss;
    oss << std::put_time(&localTime, "%d%m%Y_%H%M%S");
    const lib::String timeString = oss.str();

	return engn::Paths::Combine(engn::Paths::GetTracesPath(), timeString);
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
		const lib::String capturePath = utils::GetCaptureFilePathWithoutExtension() + ".opt";
		priv::g_currentCaptureStream = lib::File::OpenOutputStream(capturePath, lib::Flags(lib::EFileOpenFlags::Binary, lib::EFileOpenFlags::DiscardContent, lib::EFileOpenFlags::ForceCreate));

		OPTICK_SAVE_CAPTURE(&OptickProfilerImpl::SaveCaptureImpl, true);

		priv::g_currentCaptureStream.close();

		return true;
	}

private:

	static void SaveCaptureImpl(const char* data, SizeType size)
	{
		SPT_CHECK(priv::g_currentCaptureStream.is_open() && !priv::g_currentCaptureStream.bad());

		if (data)
		{
			priv::g_currentCaptureStream.write(data, size);
		}
	}
};

#endif // WITH_OPTICK

#if WITH_OPTICK

using ProfilerImpl = OptickProfilerImpl;

#else

using ProfilerImpl = NullProfilerImpl;

#endif // WITH_OPTICK

} // impl

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
