#include "Profiler.h"
#include "Paths.h"
#include "FileSystem/File.h"

#include <ctime>
#include <iomanip>
#include "Engine.h"

namespace spt::prf
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// BufferView ====================================================================================

namespace priv
{

// probably doesn't need to be threadsafe
static std::ofstream	g_currentCaptureStream;

} // priv

//////////////////////////////////////////////////////////////////////////////////////////////////
// BufferView ====================================================================================

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

class NullProfilerBackend
{
public:

	static void StartCapture() {}

	static void StopCapture() {}

	static Bool SaveCapture() { return false; }
};

#if WITH_OPTICK

class OptickProfilerBackend
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

		OPTICK_SAVE_CAPTURE(&OptickProfilerBackend::SaveCaptureImpl, true);

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

using ProfilerBackend = OptickProfilerBackend;

#else

using ProfilerBackend = NullProfilerBackend;

#endif // WITH_OPTICK

} // impl

//////////////////////////////////////////////////////////////////////////////////////////////////
// BufferView ====================================================================================

Profiler& Profiler::Get()
{
	static Profiler instance;
	return instance;
}

void Profiler::Initialize()
{
	engn::Engine::Get().GetOnBeginFrameDelegate().AddRawMember(this, &Profiler::OnBeginNewFrame);
}

void Profiler::StartCapture()
{
	impl::ProfilerBackend::StartCapture();
	m_startedCapture = true;
}

void Profiler::StopCapture()
{
	impl::ProfilerBackend::StopCapture();
	m_startedCapture = false;
}

Bool Profiler::StartedCapture() const
{
	return m_startedCapture;
}

Bool Profiler::SaveCapture()
{
	return impl::ProfilerBackend::SaveCapture();
}

Real32 Profiler::GetFrameTime(SizeType idx) const
{
	SPT_CHECK(idx < GetCollectedFrameTimesNum());

	const SizeType finalIdx = (m_oldestFrameTimeIdx + idx) % m_recentFrameTimes.size();
	return m_recentFrameTimes[finalIdx];
}

SizeType Profiler::GetCollectedFrameTimesNum() const
{
	return m_recentFrameTimesNum;
}

Real32 Profiler::GetAverageFrameTime() const
{
	return m_recentFrameTimesNum > 0 ? m_recentFrameTimesSum / static_cast<Real32>(m_recentFrameTimesNum) : 0.f;
}

Profiler::Profiler()
	: m_startedCapture(false)
	, m_recentFrameTimesNum(0)
	, m_oldestFrameTimeIdx(0)
	, m_recentFrameTimesSum(0.f)
{ }

void Profiler::OnBeginNewFrame()
{
	SPT_PROFILER_FUNCTION();

	const engn::EngineTimer& engineTimer = engn::Engine::Get().GetEngineTimer();
	const Real32 deltaTime = engineTimer.GetDeltaTime();

	const SizeType newFrameTimeIdx = (m_oldestFrameTimeIdx + m_recentFrameTimesNum) % m_recentFrameTimes.size();

	if (m_recentFrameTimesNum < m_recentFrameTimes.size())
	{
		++m_recentFrameTimesNum;
	}
	else
	{
		m_recentFrameTimesSum -= m_recentFrameTimes[m_oldestFrameTimeIdx];
		m_oldestFrameTimeIdx = (m_oldestFrameTimeIdx + 1) % m_recentFrameTimes.size();
	}
	
	m_recentFrameTimes[newFrameTimeIdx] = deltaTime;
	m_recentFrameTimesSum += deltaTime;
}

} // spt::prf
