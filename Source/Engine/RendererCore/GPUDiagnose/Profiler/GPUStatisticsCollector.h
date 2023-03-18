#pragma once

#include "RendererCoreMacros.h"
#include "SculptorCoreTypes.h"
#include "RHICore/RHIQueryTypes.h"


namespace spt::rdr
{

class QueryPool;
class CommandRecorder;


enum class EQueryFlags
{
	None = 0,

	Default = None
};


struct StatisticsContext
{
	lib::DynamicArray<Uint64> timestamps;
};


struct GPUStatisticsScopeResult
{
	GPUStatisticsScopeResult()
		: durationInMs(0.0f)
	{ }

	lib::HashedString name;
	Real32 durationInMs;

	lib::DynamicArray<GPUStatisticsScopeResult> children;
};


struct GPUStatisticsScopeDefinition
{
	GPUStatisticsScopeDefinition(const lib::HashedString& inName, Uint32 inBeginTimestampIdx)
		: name(inName)
		, beginTimestampIndex(inBeginTimestampIdx)
		, endTimestampIndex(idxNone<Uint32>)
	{ }

	GPUStatisticsScopeResult EmitScopeResult(const StatisticsContext& context) const
	{
		GPUStatisticsScopeResult result;

		FillScopeResultData(context, INOUT result);

		result.children.reserve(children.size());
		std::transform(std::cbegin(children), std::cend(children),
					   std::back_inserter(result.children),
					   [&context](const GPUStatisticsScopeDefinition& definition)
					   {
						   return definition.EmitScopeResult(context);
					   });

		return result;
	}

	lib::HashedString name;

	Uint32 beginTimestampIndex;
	Uint32 endTimestampIndex;

	lib::DynamicArray<GPUStatisticsScopeDefinition> children;

private:

	void FillScopeResultData(const StatisticsContext& context, INOUT GPUStatisticsScopeResult& result) const
	{
		result.name = name;

		const Uint64 beginTimestamp = context.timestamps[beginTimestampIndex];
		const Uint64 endTimestamp = context.timestamps[endTimestampIndex];

		result.durationInMs = static_cast<Real32>(endTimestamp - beginTimestamp) / 1000000.0f;
	}
};


class RENDERER_CORE_API GPUStatisticsCollector
{
public:

	GPUStatisticsCollector();

	void BeginScope(CommandRecorder& recoder, const lib::HashedString& scopeName, EQueryFlags queryFlags);
	void EndScope(CommandRecorder& recoder);

	GPUStatisticsScopeResult CollectStatistics();

private:

	lib::SharedRef<QueryPool> CreateQueryPool(rhi::EQueryType type, Uint32 queryCount) const;

	lib::SharedRef<QueryPool> m_timestampsQueryPool;
	Uint32 m_timestampsQueryPoolIndex;

	lib::DynamicArray<GPUStatisticsScopeDefinition> m_scopeDefinitions;

	lib::DynamicArray<GPUStatisticsScopeDefinition*> m_scopesInProgressStack;
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

#if RENDERER_VALIDATION

#define SPT_GPU_STATISTICS_SCOPE_NAME SPT_SCOPE_NAME(_gpu_statistics_scope_)

#define SPT_GPU_STATISTICS_SCOPE_FLAGS(recorderRef, collector, name, flags)	const rdr::GPUStatisticsScope SPT_GPU_STATISTICS_SCOPE_NAME (recorderRef, collector, name, flags);
#define SPT_GPU_STATISTICS_SCOPE(recorderRef, collector, name)				const rdr::GPUStatisticsScope SPT_GPU_STATISTICS_SCOPE_NAME (recorderRef, collector, name);

#else

#define SPT_GPU_STATISTICS_SCOPE_FLAGS(recorderRef, collector, name, flags)
#define SPT_GPU_STATISTICS_SCOPE(recorderRef, collector, name)

#endif // RENDERER_VALIDATION

} // spt::rdr