#pragma once

#include "RenderGraphMacros.h"
#include "SculptorCoreTypes.h"


#define RG_ENABLE_DIAGNOSTICS (SPT_DEBUG || SPT_DEVELOPMENT)


namespace spt::rdr
{
class CommandRecorder;
class GPUStatisticsCollector;
} // spt::rdr


#if RG_ENABLE_DIAGNOSTICS

namespace spt::rg
{

class RenderGraphBuilder;


using RGDiagnosticsRecord = lib::DynamicArray<lib::HashedString>;


class RGProfilerRecorder
{
public:

	RGProfilerRecorder();

	void BeginScope(lib::HashedString name);
	void PopScope();

	const RGDiagnosticsRecord& GetRecord() const { return m_recordState; }

private:

	RGDiagnosticsRecord m_recordState;
};


class RGDiagnosticsPlayback
{
public:

	RGDiagnosticsPlayback();
	
	void Playback(rdr::CommandRecorder& recorder, const lib::SharedPtr<rdr::GPUStatisticsCollector>& statisticsCollector, const RGDiagnosticsRecord& record);
	void PopRemainingScopes(rdr::CommandRecorder& recorder, const lib::SharedPtr<rdr::GPUStatisticsCollector>& statisticsCollector);

private:

	void PlaybackPush(rdr::CommandRecorder& recorder, const lib::SharedPtr<rdr::GPUStatisticsCollector>& statisticsCollector, lib::HashedString scopeName);
	void PlaybackPop(rdr::CommandRecorder& recorder, const lib::SharedPtr<rdr::GPUStatisticsCollector>& statisticsCollector);

	RGDiagnosticsRecord m_recordState;
};



class RENDER_GRAPH_API RGDiagnosticsScope
{
public:

	RGDiagnosticsScope(RenderGraphBuilder& graphBuilder, lib::HashedString name);
	~RGDiagnosticsScope();

public:

	RenderGraphBuilder& m_graphBuilder;
};

} // spt::rg


#define SPT_RG_DIAGNOSTICS_SCOPE_NAME SPT_SCOPE_NAME(rgDiagnosticsScope)
#define SPT_RG_DIAGNOSTICS_SCOPE(graphBuilder, name)	const spt::rg::RGDiagnosticsScope SPT_RG_DIAGNOSTICS_SCOPE_NAME (graphBuilder, name);

#else

#define SPT_RG_DIAGNOSTICS_SCOPE(graphBuilder, name)

#endif // RG_ENABLE_DIAGNOSTICS
