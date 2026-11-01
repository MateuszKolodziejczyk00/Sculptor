#pragma once

#include "Pipelines/PipelineState.h"
#include "RendererCoreMacros.h"
#include "SculptorCoreTypes.h"
#include "RHICore/RHIQueryTypes.h"
#include "GPUStatisticsTypes.h"


namespace spt::rdr
{

class QueryPool;
class CommandRecorder;

enum class EQueryFlags
{
	None			= 0,

	Rasterization	= BIT(0),
	Compute			= BIT(1),

	Default = None
};


struct StatisticsContext
{
	lib::DynamicArray<GPUTimestampNs> timestamps;
	lib::DynamicArray<Uint64>         pipelineStatistics;
	struct GPUStatisticsData*         statisticsData = nullptr;
};


struct GPUStatisticsScopeData
{
	GPUStatisticsScopeData()
	{ }

	Bool IsValid() const
	{
		return name.IsValid();
	}

	GPUDurationNs GetDuration() const
	{
		return endTimestamp - beginTimestamp;
	}

	GPUDurationMs GetDurationInMs() const
	{
		return GetDuration() / 1000000.0;
	}

	lib::HashedString name;

	GPUTimestampNs beginTimestamp = 0u;
	GPUTimestampNs endTimestamp   = 0u;

	std::optional<Uint32> taskShaderInvocations;
	std::optional<Uint32> meshShaderInvocations;
	std::optional<Uint32> fragmentShaderInvocations;
	std::optional<Uint32> computeShaderInvocations;

	rhi::PipelineStatistics psoStatistics;

	std::optional<Uint64> transientMemoryUsage;

	lib::Span<TransientAllocEventInfo> transientAllocations;
	lib::Span<TransientAllocEventInfo> transientDeallocations;

	lib::Span<GPUStatisticsScopeData> children;
};


struct GPUStatisticsData
{
	GPUStatisticsData() = default;

	GPUStatisticsData(const GPUStatisticsData&) = delete;
	GPUStatisticsData& operator=(const GPUStatisticsData&) = delete;

	GPUStatisticsData(GPUStatisticsData&&) = delete;
	GPUStatisticsData& operator=(GPUStatisticsData&&) = delete;

	void Reset()
	{
		scopesPool.Clear();
		transientAllocationsPool.Clear();
		transientDeallocationsPool.Clear();
		fullScope = nullptr;
	}

	lib::InlineDynamicArray<GPUStatisticsScopeData, 1024u> scopesPool;

	lib::InlineDynamicArray<TransientAllocEventInfo, 1024u> transientAllocationsPool;
	lib::InlineDynamicArray<TransientAllocEventInfo, 1024u> transientDeallocationsPool;

	GPUStatisticsScopeData* fullScope = nullptr;
};


struct GPUStatisticsScopeDefinition
{
	GPUStatisticsScopeDefinition(const lib::HashedString& inName, Uint32 inBeginTimestampIdx)
		: name(inName)
		, beginTimestampIndex(inBeginTimestampIdx)
		, endTimestampIndex(idxNone<Uint32>)
	{ }

	void EmitScopeResult(const StatisticsContext& context, GPUStatisticsScopeData& scopeData) const;

	lib::HashedString name;

	Uint32 beginTimestampIndex;
	Uint32 endTimestampIndex;

	std::optional<Uint32> pipelineStatisticsQueryIdx;
	
	std::optional<Uint32> taskShaderInvocationsIdx;
	std::optional<Uint32> meshShaderInvocationsIdx;
	std::optional<Uint32> fragmentShaderInvocationsIdx;
	std::optional<Uint32> computeShaderInvocationsIdx;

	rdr::PipelineStateID statisticsPSO;

	std::optional<Uint64> transientMemoryUsage;

	lib::Span<TransientAllocEventInfo> transientAllocations;
	lib::Span<TransientAllocEventInfo> transientDeallocations;

	lib::DynamicArray<GPUStatisticsScopeDefinition> children;

private:

	void FillScopeResultData(const StatisticsContext& context, INOUT GPUStatisticsScopeData& result) const;
};


class RENDERER_CORE_API GPUStatisticsCollector
{
public:

	GPUStatisticsCollector();

	void SetCollectMemoryDetails(Bool collectMemoryDetails) { m_collectMemoryDetails = collectMemoryDetails; }

	void BeginScope(CommandRecorder& recoder, const lib::HashedString& scopeName, EQueryFlags queryFlags);
	void EndScope(CommandRecorder& recoder);

	void SetScopeStatisticsPSO(rdr::PipelineStateID psoID);

	void SetScopeTransientMemoryUsage(Uint64 transientMemoryUsage);
	void AddScopeTransientAllocation(const TransientAllocEventInfo& allocation);
	void AddScopeTransientDeallocation(const TransientAllocEventInfo& deallocation);

	void CollectStatistics(OUT GPUStatisticsData& outStatistics);

private:

	lib::SharedRef<QueryPool> CreateQueryPool(rhi::EQueryType type, Uint32 queryCount, rhi::EQueryStatisticsType statisticsType = rhi::EQueryStatisticsType::None) const;

	lib::SharedRef<QueryPool> m_timestampsQueryPool;
	Uint32 m_timestampsQueryPoolIndex;

	lib::SharedRef<QueryPool> m_pipelineStatsQueryPool;
	Uint32 m_pipelineStatsQueryPoolIndex;

	lib::DynamicArray<GPUStatisticsScopeDefinition> m_scopeDefinitions;

	lib::InlineDynamicArray<TransientAllocEventInfo, 1024u> m_transientAllocations;
	lib::InlineDynamicArray<TransientAllocEventInfo, 1024u> m_transientDeallocations;

	/** This will never be dangling pointers because they can point only to last element of array and this array won't be modified during lifetime of pointer which is stored here */
	lib::DynamicArray<GPUStatisticsScopeDefinition*> m_scopesInProgressStack;

	Bool m_collectMemoryDetails = false;
};


class RENDERER_CORE_API GPUStatisticsScope
{
public:

	GPUStatisticsScope(CommandRecorder& recorder, const lib::SharedPtr<GPUStatisticsCollector>& collector, const lib::HashedString& regionName, EQueryFlags queryFlags = EQueryFlags::Default);
	~GPUStatisticsScope();

private:

	CommandRecorder&		m_recorder;
	GPUStatisticsCollector* m_collector;
};

#if SPT_ENABLE_PROFILER

#define SPT_GPU_STATISTICS_SCOPE_NAME SPT_SCOPE_NAME(_gpu_statistics_scope_)

#define SPT_GPU_STATISTICS_SCOPE_FLAGS(recorderRef, collector, name, flags)	const rdr::GPUStatisticsScope SPT_GPU_STATISTICS_SCOPE_NAME (recorderRef, collector, name, flags);
#define SPT_GPU_STATISTICS_SCOPE(recorderRef, collector, name)				const rdr::GPUStatisticsScope SPT_GPU_STATISTICS_SCOPE_NAME (recorderRef, collector, name);

#else

#define SPT_GPU_STATISTICS_SCOPE_FLAGS(recorderRef, collector, name, flags)
#define SPT_GPU_STATISTICS_SCOPE(recorderRef, collector, name)

#endif // SPT_ENABLE_PROFILER

} // spt::rdr
