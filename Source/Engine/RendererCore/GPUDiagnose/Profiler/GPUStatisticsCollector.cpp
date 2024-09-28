#include "GPUStatisticsCollector.h"
#include "Types/QueryPool.h"
#include "ResourcesManager.h"
#include "CommandsRecorder/CommandRecorder.h"

namespace spt::rdr
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// GPUStatisticsCollector ========================================================================

GPUStatisticsCollector::GPUStatisticsCollector()
	: m_timestampsQueryPool(CreateQueryPool(rhi::EQueryType::Timestamp, 1024))
	, m_timestampsQueryPoolIndex(0)
	, m_pipelineStatsQueryPool(CreateQueryPool(rhi::EQueryType::Statistics, 1024, rhi::EQueryStatisticsType::All))
	, m_pipelineStatsQueryPoolIndex(0)

{
	m_timestampsQueryPool->Reset();
	m_pipelineStatsQueryPool->Reset();
}

void GPUStatisticsCollector::BeginScope(CommandRecorder& recoder, const lib::HashedString& scopeName, EQueryFlags queryFlags)
{
	lib::DynamicArray<GPUStatisticsScopeDefinition>& currentLevelScopes = !m_scopesInProgressStack.empty() ? m_scopesInProgressStack.back()->children : m_scopeDefinitions;
	
	const Uint32 beginTimestampIndex = m_timestampsQueryPoolIndex++;

	recoder.WriteTimestamp(m_timestampsQueryPool, beginTimestampIndex, rhi::EPipelineStage::TopOfPipe);

	GPUStatisticsScopeDefinition& newScope = currentLevelScopes.emplace_back(GPUStatisticsScopeDefinition(scopeName, beginTimestampIndex));

	if (queryFlags != EQueryFlags::None)
	{
		constexpr Uint32 statisticsNum = 5;
		
		const Uint32 statisticsBeginIndex = m_pipelineStatsQueryPoolIndex * statisticsNum;
		recoder.BeginQuery(m_pipelineStatsQueryPool, m_pipelineStatsQueryPoolIndex);

		newScope.pipelineStatisticsQueryIdx = m_pipelineStatsQueryPoolIndex++;

		if (lib::HasAnyFlag(queryFlags, EQueryFlags::Rasterization))
		{
			newScope.inputAsseblyVerticesIdx		= statisticsBeginIndex;
			newScope.inputAsseblyPrimitivesIdx		= statisticsBeginIndex + 1;
			newScope.vertexShaderInvocationsIdx		= statisticsBeginIndex + 2;
			newScope.fragmentShaderInvocationsIdx	= statisticsBeginIndex + 3;
		}

		if (lib::HasAnyFlag(queryFlags, EQueryFlags::Compute))
		{
			newScope.computeShaderInvocationsIdx = statisticsBeginIndex + 4;
		}
	}

	m_scopesInProgressStack.emplace_back(&newScope);
}

void GPUStatisticsCollector::EndScope(CommandRecorder& recoder)
{
	SPT_CHECK(!m_scopesInProgressStack.empty());

	GPUStatisticsScopeDefinition& currentScope = *m_scopesInProgressStack.back();

	const Uint32 endTimestampIndex = m_timestampsQueryPoolIndex++;
	recoder.WriteTimestamp(m_timestampsQueryPool, endTimestampIndex, rhi::EPipelineStage::BottomOfPipe);

	currentScope.endTimestampIndex = endTimestampIndex;

	if (currentScope.pipelineStatisticsQueryIdx.has_value())
	{
		recoder.EndQuery(m_pipelineStatsQueryPool, currentScope.pipelineStatisticsQueryIdx.value());
	}

	m_scopesInProgressStack.pop_back();
}

GPUStatisticsScopeData GPUStatisticsCollector::CollectStatistics()
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(m_scopesInProgressStack.empty());

	GPUStatisticsScopeData result;

	StatisticsContext context;
	context.timestamps			= m_timestampsQueryPool->GetRHI().GetResults(m_timestampsQueryPoolIndex);
	context.pipelineStatistics	= m_pipelineStatsQueryPool->GetRHI().GetResults(m_pipelineStatsQueryPoolIndex);

	if (!context.timestamps.empty())
	{
		result.children.reserve(m_scopeDefinitions.size());
		std::transform(std::cbegin(m_scopeDefinitions), std::cend(m_scopeDefinitions),
					   std::back_inserter(result.children),
					   [ &context ](const GPUStatisticsScopeDefinition& scopeDef)
					   {
						   return scopeDef.EmitScopeResult(context);
					   });

		result.name = "GPU Frame";

		result.beginTimestamp = std::min_element(std::cbegin(result.children), std::cend(result.children),
												 [](const GPUStatisticsScopeData& lhs, const GPUStatisticsScopeData& rhs)
												 {
													 return lhs.beginTimestamp < rhs.beginTimestamp;
												 })->beginTimestamp;

		result.endTimestamp = std::max_element(std::cbegin(result.children), std::cend(result.children),
											   [](const GPUStatisticsScopeData& lhs, const GPUStatisticsScopeData& rhs)
											   {
												   return lhs.endTimestamp < rhs.endTimestamp;
											   })->endTimestamp;
	}

	return result;
}

lib::SharedRef<QueryPool> GPUStatisticsCollector::CreateQueryPool(rhi::EQueryType type, Uint32 queryCount, rhi::EQueryStatisticsType statisticsType /*= rhi::EQueryStatisticsType::None*/) const
{
	rhi::QueryPoolDefinition queryPoolDef;
	queryPoolDef.queryCount		= queryCount;
	queryPoolDef.queryType		= type;
	queryPoolDef.statisticsType	= statisticsType;
	return rdr::ResourcesManager::CreateQueryPool(queryPoolDef);
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// GPUStatisticsScope ============================================================================

GPUStatisticsScope::GPUStatisticsScope(CommandRecorder& recorder, const lib::SharedPtr<GPUStatisticsCollector>& collector, const lib::HashedString& regionName, EQueryFlags queryFlags /*= EQueryFlags::Default*/)
	: m_recorder(recorder)
	, m_collector(collector.get())
{
	if (m_collector)
	{
		m_collector->BeginScope(m_recorder, regionName, queryFlags);
	}
}

GPUStatisticsScope::~GPUStatisticsScope()
{
	if (m_collector)
	{
		m_collector->EndScope(m_recorder);
	}
}

} // spt::rdr
