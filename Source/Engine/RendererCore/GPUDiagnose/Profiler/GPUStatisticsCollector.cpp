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
{ }

void GPUStatisticsCollector::BeginScope(CommandRecorder& recoder, const lib::HashedString& scopeName, EQueryFlags queryFlags)
{
	lib::DynamicArray<GPUStatisticsScopeDefinition>& currentLevelScopes = !m_scopesInProgressStack.empty() ? m_scopesInProgressStack.back()->children : m_scopeDefinitions;
	
	const Uint32 beginTimestampIndex = m_timestampsQueryPoolIndex++;

	if (beginTimestampIndex == 0)
	{
		recoder.ResetQueryPool(m_timestampsQueryPool, 0, m_timestampsQueryPool->GetRHI().GetQueryCount());
	}

	recoder.WriteTimestamp(m_timestampsQueryPool, beginTimestampIndex, rhi::EPipelineStage::TopOfPipe);

	GPUStatisticsScopeDefinition& newScope = currentLevelScopes.emplace_back(GPUStatisticsScopeDefinition(scopeName, beginTimestampIndex));

	m_scopesInProgressStack.emplace_back(&newScope);
}

void GPUStatisticsCollector::EndScope(CommandRecorder& recoder)
{
	SPT_CHECK(!m_scopesInProgressStack.empty());

	GPUStatisticsScopeDefinition& currentScope = *m_scopesInProgressStack.back();

	const Uint32 endTimestampIndex = m_timestampsQueryPoolIndex++;
	recoder.WriteTimestamp(m_timestampsQueryPool, endTimestampIndex, rhi::EPipelineStage::BottomOfPipe);

	currentScope.endTimestampIndex = endTimestampIndex;

	m_scopesInProgressStack.pop_back();
}

GPUStatisticsScopeResult GPUStatisticsCollector::CollectStatistics()
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(m_scopesInProgressStack.empty());

	GPUStatisticsScopeResult result;

	StatisticsContext context;
	context.timestamps = m_timestampsQueryPool->GetRHI().GetResults(m_timestampsQueryPoolIndex);

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
		
		result.durationInMs = std::accumulate(std::cbegin(result.children), std::cend(result.children),
											  0.f,
											  [](Real32 currentTime, const GPUStatisticsScopeResult& scopeResult)
											  {
												  return currentTime + scopeResult.durationInMs;
											  });
	}

	return result;
}

lib::SharedRef<QueryPool> GPUStatisticsCollector::CreateQueryPool(rhi::EQueryType type, Uint32 queryCount) const
{
	rhi::QueryPoolDefinition queryPoolDef;
	queryPoolDef.queryCount = queryCount;
	queryPoolDef.queryType	= type;
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
