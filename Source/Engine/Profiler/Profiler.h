#pragma once

#include "ProfilerMacros.h"
#include "SculptorCoreTypes.h"
#include "Delegates/MulticastDelegate.h"
#include "GPUDiagnose/Profiler/GPUStatisticsCollector.h"


namespace spt::rdr
{
class GPUStatisticsCollector;
} // spt::rdr


namespace spt::prf
{

struct GPUProfilerStatistics
{
	GPUProfilerStatistics() = default;

	Bool IsValid() const
	{
		return resolution != math::Vector2u::Zero() && frameStatistics.IsValid();
	}

	rdr::GPUDurationMs GetGPUFrameDuration() const
	{
		return frameStatistics.GetDurationInMs();
	}

	math::Vector2u resolution = math::Vector2u::Zero();

	rdr::GPUStatisticsScopeData frameStatistics;
};


struct ScopeMetrics
{
	Uint64             invocationsNum = 0u;
	rdr::GPUDurationNs durationSum    = 0u;
};


class PROFILER_API Profiler
{
public:

	static Profiler& Get();

	void Initialize();

	// Metrics ====================================================

	ScopeMetrics GetScopeMetrics(const lib::HashedString& scopeName) const;
	void ResetScopeMetrics();

	// Frame Time =================================================

	Real32 GetFrameTime(SizeType idx) const;
	SizeType GetCollectedFrameTimesNum() const;
	Real32 GetAverageFrameTime() const;

	// GPU ========================================================

	void SetGPUFrameStatistics(GPUProfilerStatistics frameStatistics);

	void FlushNewGPUFrameStatistics();

	const GPUProfilerStatistics& GetGPUFrameStatistics() const;

private:

	Profiler();

	void OnBeginNewFrame();

	void FlushScopeMetrics(const rdr::GPUStatisticsScopeData& scope);

	Bool m_startedCapture;

	lib::StaticArray<Real32, 100> m_recentFrameTimes;
	SizeType                      m_recentFrameTimesNum;
	SizeType                      m_oldestFrameTimeIdx;
	Real32                        m_recentFrameTimesSum;

	lib::DelegateHandle m_captureDelegateHandle;

	GPUProfilerStatistics m_gpuFrameStatistics;

	std::optional<GPUProfilerStatistics> m_newFrameStatistics;
	mutable lib::Lock                    m_newFrameStatisticsLock;


	lib::HashMap<lib::HashedString, ScopeMetrics> m_scopeMetrics;
	mutable lib::Lock                             m_scopeMetricsLock;
};

} // spt::prf