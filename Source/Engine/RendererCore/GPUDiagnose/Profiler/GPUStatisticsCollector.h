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
	None			= 0,

	Rasterization	= BIT(0),
	Compute			= BIT(1),

	Default = None
};


struct StatisticsContext
{
	lib::DynamicArray<Uint64> timestamps;
	lib::DynamicArray<Uint64> pipelineStatistics;
};


struct GPUStatisticsScopeResult
{
	GPUStatisticsScopeResult()
		: durationInMs(0.0f)
	{ }

	Bool IsValid() const
	{
		return name.IsValid();
	}

	lib::HashedString name;
	Real32 durationInMs;

	std::optional<Uint32> inputAsseblyVertices;
	std::optional<Uint32> inputAsseblyPrimitives;
	
	std::optional<Uint32> vertexShaderInvocations;
	std::optional<Uint32> fragmentShaderInvocations;
	std::optional<Uint32> computeShaderInvocations;

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

	std::optional<Uint32> pipelineStatisticsQueryIdx;

	std::optional<Uint32> inputAsseblyVerticesIdx;
	std::optional<Uint32> inputAsseblyPrimitivesIdx;
	
	std::optional<Uint32> vertexShaderInvocationsIdx;
	std::optional<Uint32> fragmentShaderInvocationsIdx;
	std::optional<Uint32> computeShaderInvocationsIdx;

	lib::DynamicArray<GPUStatisticsScopeDefinition> children;

private:

	void FillScopeResultData(const StatisticsContext& context, INOUT GPUStatisticsScopeResult& result) const
	{
		result.name = name;

		const Uint64 beginTimestamp = context.timestamps[beginTimestampIndex];
		const Uint64 endTimestamp = context.timestamps[endTimestampIndex];

		result.durationInMs = static_cast<Real32>(endTimestamp - beginTimestamp) / 1000000.0f;

		if (inputAsseblyVerticesIdx.has_value())
		{
			result.inputAsseblyVertices = static_cast<Uint32>(context.pipelineStatistics[inputAsseblyVerticesIdx.value()]);
		}
		if (inputAsseblyPrimitivesIdx.has_value())
		{
			result.inputAsseblyPrimitives = static_cast<Uint32>(context.pipelineStatistics[inputAsseblyPrimitivesIdx.value()]);
		}
		if (vertexShaderInvocationsIdx.has_value())
		{
			result.vertexShaderInvocations = static_cast<Uint32>(context.pipelineStatistics[vertexShaderInvocationsIdx.value()]);
		}
		if (fragmentShaderInvocationsIdx.has_value())
		{
			result.fragmentShaderInvocations = static_cast<Uint32>(context.pipelineStatistics[fragmentShaderInvocationsIdx.value()]);
		}
		if (computeShaderInvocationsIdx.has_value())
		{
			result.computeShaderInvocations = static_cast<Uint32>(context.pipelineStatistics[computeShaderInvocationsIdx.value()]);
		}
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

	lib::SharedRef<QueryPool> CreateQueryPool(rhi::EQueryType type, Uint32 queryCount, rhi::EQueryStatisticsType statisticsType = rhi::EQueryStatisticsType::None) const;

	lib::SharedRef<QueryPool> m_timestampsQueryPool;
	Uint32 m_timestampsQueryPoolIndex;

	lib::SharedRef<QueryPool> m_pipelineStatsQueryPool;
	Uint32 m_pipelineStatsQueryPoolIndex;

	lib::DynamicArray<GPUStatisticsScopeDefinition> m_scopeDefinitions;

	/** This will never be dangling pointers because they can point only to last element of array and this array won't be modified during lifetime of pointer which is stored here */
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

#if ENABLE_PROFILER

#define SPT_GPU_STATISTICS_SCOPE_NAME SPT_SCOPE_NAME(_gpu_statistics_scope_)

#define SPT_GPU_STATISTICS_SCOPE_FLAGS(recorderRef, collector, name, flags)	const rdr::GPUStatisticsScope SPT_GPU_STATISTICS_SCOPE_NAME (recorderRef, collector, name, flags);
#define SPT_GPU_STATISTICS_SCOPE(recorderRef, collector, name)				const rdr::GPUStatisticsScope SPT_GPU_STATISTICS_SCOPE_NAME (recorderRef, collector, name);

#else

#define SPT_GPU_STATISTICS_SCOPE_FLAGS(recorderRef, collector, name, flags)
#define SPT_GPU_STATISTICS_SCOPE(recorderRef, collector, name)

#endif // ENABLE_PROFILER

} // spt::rdr