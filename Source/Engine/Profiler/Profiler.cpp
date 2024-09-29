#include "Profiler.h"
#include "Paths.h"
#include "FileSystem/File.h"
#include "Engine.h"

#include <ctime>
#include <iomanip>

namespace spt::prf
{

Profiler& Profiler::Get()
{
	static Profiler instance;
	return instance;
}

void Profiler::Initialize()
{
	engn::Engine::Get().GetOnBeginFrameDelegate().AddRawMember(this, &Profiler::OnBeginNewFrame);
}

ScopeMetrics Profiler::GetScopeMetrics(const lib::HashedString& scopeName) const
{
	const lib::LockGuard lockGuard(m_scopeMetricsLock);

	const auto it = m_scopeMetrics.find(scopeName);
	return it != m_scopeMetrics.cend() ? it->second : ScopeMetrics{};
}

void Profiler::ResetScopeMetrics()
{
	const lib::LockGuard lockGuard(m_scopeMetricsLock);

	m_scopeMetrics.clear();
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

void Profiler::SetGPUFrameStatistics(GPUProfilerStatistics frameStatistics)
{
	const lib::LockGuard lock(m_newFrameStatisticsLock);
	m_newFrameStatistics = std::move(frameStatistics);
}

void Profiler::FlushNewGPUFrameStatistics()
{
	const lib::LockGuard lock(m_newFrameStatisticsLock);
	if (m_newFrameStatistics)
	{
		m_gpuFrameStatistics = std::move(*m_newFrameStatistics);
		m_newFrameStatistics = std::nullopt;
	}

	{
		const lib::LockGuard lockGuard(m_scopeMetricsLock);
		FlushScopeMetrics(m_gpuFrameStatistics.frameStatistics);
	}
}

const GPUProfilerStatistics& Profiler::GetGPUFrameStatistics() const
{
	return m_gpuFrameStatistics;
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

void Profiler::FlushScopeMetrics(const rdr::GPUStatisticsScopeData& scope)
{
	ScopeMetrics& metrics = m_scopeMetrics[scope.name];

	metrics.invocationsNum += 1u;
	metrics.durationSum += scope.GetDuration();

	for (const rdr::GPUStatisticsScopeData& childScope : scope.children)
	{
		FlushScopeMetrics(childScope);
	}
}

} // spt::prf
