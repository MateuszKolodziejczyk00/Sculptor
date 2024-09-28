#include "RGDiagnostics.h"

#if RG_ENABLE_DIAGNOSTICS

#include "GPUDiagnose/Profiler/GPUStatisticsCollector.h"
#include "RenderGraphBuilder.h"


namespace spt::rg
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// RGProfilerRecorder ============================================================================

RGProfilerRecorder::RGProfilerRecorder()
{ }

void RGProfilerRecorder::BeginScope(lib::HashedString name)
{
	m_recordState.emplace_back(name);
}

void RGProfilerRecorder::PopScope()
{
	m_recordState.pop_back();
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// RGProfilerPlayback ============================================================================

RGDiagnosticsPlayback::RGDiagnosticsPlayback()
{
}

void RGDiagnosticsPlayback::Playback(rdr::CommandRecorder& recorder, const lib::SharedPtr<rdr::GPUStatisticsCollector>& statisticsCollector, const RGDiagnosticsRecord& record)
{
	const auto areScopesMatching = [this, &record](SizeType idx)
		{
			if (idx >= record.size() || idx >= m_recordState.size())
			{
				return false;
			}

			return record[idx] == m_recordState[idx];
		};

	SizeType scopeIdx = 0u;

	while (areScopesMatching(scopeIdx))
	{
		++scopeIdx;
	}

	while (scopeIdx < m_recordState.size())
	{
		PlaybackPop(recorder, statisticsCollector);
	}

	while (scopeIdx < record.size())
	{
		PlaybackPush(recorder, statisticsCollector, record[scopeIdx]);
		++scopeIdx;
	}

}

void RGDiagnosticsPlayback::PopRemainingScopes(rdr::CommandRecorder& recorder, const lib::SharedPtr<rdr::GPUStatisticsCollector>& statisticsCollector)
{
	while (!m_recordState.empty())
	{
		PlaybackPop(recorder, statisticsCollector);
	}
}

void RGDiagnosticsPlayback::PlaybackPush(rdr::CommandRecorder& recorder, const lib::SharedPtr<rdr::GPUStatisticsCollector>& statisticsCollector, lib::HashedString scopeName)
{
#if RENDERER_VALIDATION
	recorder.BeginDebugRegion(scopeName, lib::Color(static_cast<Uint32>(scopeName.GetKey())));
#endif // RENDERER_VALIDATION

	if (statisticsCollector)
	{
		statisticsCollector->BeginScope(recorder, scopeName, rdr::EQueryFlags::Default);
	}

	m_recordState.emplace_back(scopeName);
}

void RGDiagnosticsPlayback::PlaybackPop(rdr::CommandRecorder& recorder, const lib::SharedPtr<rdr::GPUStatisticsCollector>& statisticsCollector)
{
	if (statisticsCollector)
	{
		statisticsCollector->EndScope(recorder);
	}

#if RENDERER_VALIDATION
	recorder.EndDebugRegion();
#endif // RENDERER_VALIDATION

	m_recordState.pop_back();
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// RGProfilerScope ===============================================================================

RGDiagnosticsScope::RGDiagnosticsScope(RenderGraphBuilder& graphBuilder, lib::HashedString name)
	: m_graphBuilder(graphBuilder)
{
	m_graphBuilder.PushProfilerScope(name);
}

RGDiagnosticsScope::~RGDiagnosticsScope()
{
	m_graphBuilder.PopProfilerScope();
}

} // spt::rg

#endif // RG_ENABLE_DIAGNOSTICS
