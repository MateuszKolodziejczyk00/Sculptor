#include "GPUStatisticsCollector.h"
#include "Types/QueryPool.h"
#include "ResourcesManager.h"
#include "CommandsRecorder/CommandRecorder.h"
#include "Types/Pipeline/Pipeline.h"

namespace spt::rdr
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// Cosnts ========================================================================================

namespace consts
{
// Skip mesh shader on purpose, from some reason it tanks performance on my GPU (RTX 4080, driver 616.92)
static constexpr rhi::EQueryStatisticsType queryFlags = lib::Flags(rhi::EQueryStatisticsType::TSInvocations, rhi::EQueryStatisticsType::TSInvocations, rhi::EQueryStatisticsType::FSInvocations, rhi::EQueryStatisticsType::CSInvocations);
constexpr Uint32 statisticsNum = 3u;
} // namespace consts

//////////////////////////////////////////////////////////////////////////////////////////////////
// GPUStatisticsCollector ========================================================================

void GPUStatisticsScopeDefinition::EmitScopeResult(const StatisticsContext& context, GPUStatisticsScopeData& scopeData) const
{
	FillScopeResultData(context, INOUT scopeData);

	scopeData.children = context.statisticsData->scopesPool.EmplaceRangeUninitialized(children.size());

	for (SizeType childIdx = 0u; childIdx < children.size(); ++childIdx)
	{
		const GPUStatisticsScopeDefinition& childScopeDef = children[childIdx];
		GPUStatisticsScopeData& childScopeData = scopeData.children[childIdx];

		childScopeDef.EmitScopeResult(context, INOUT childScopeData);
	}
}

void GPUStatisticsScopeDefinition::FillScopeResultData(const StatisticsContext& context, INOUT GPUStatisticsScopeData& result) const
{
	result.name = name;

	const GPUTimestampNs beginTimestamp = context.timestamps[beginTimestampIndex];
	const GPUTimestampNs endTimestamp   = context.timestamps[endTimestampIndex];

	SPT_CHECK(beginTimestamp <= endTimestamp);

	result.beginTimestamp = beginTimestamp;
	result.endTimestamp   = endTimestamp;

	if (taskShaderInvocationsIdx.has_value())
	{
		result.taskShaderInvocations = static_cast<Uint32>(context.pipelineStatistics[taskShaderInvocationsIdx.value()]);
	}
	if (meshShaderInvocationsIdx.has_value())
	{
		result.meshShaderInvocations = static_cast<Uint32>(context.pipelineStatistics[meshShaderInvocationsIdx.value()]);
	}
	if (fragmentShaderInvocationsIdx.has_value())
	{
		result.fragmentShaderInvocations = static_cast<Uint32>(context.pipelineStatistics[fragmentShaderInvocationsIdx.value()]);
	}
	if (computeShaderInvocationsIdx.has_value())
	{
		result.computeShaderInvocations = static_cast<Uint32>(context.pipelineStatistics[computeShaderInvocationsIdx.value()]);
	}

	if (statisticsPSO.IsValid())
	{
		const lib::SharedPtr<rdr::Pipeline> statisticsPipeline = rdr::ResourcesManager::GetPipelineObject(statisticsPSO);
		SPT_CHECK(!!statisticsPipeline);
		result.psoStatistics = statisticsPipeline->GetRHI().GetPipelineStatistics();
	}

	if (transientMemoryUsage.has_value())
	{
		result.transientMemoryUsage = *transientMemoryUsage;
	}

	result.transientAllocations   = context.statisticsData->transientAllocationsPool.EmplaceRange(transientAllocations);
	result.transientDeallocations = context.statisticsData->transientDeallocationsPool.EmplaceRange(transientDeallocations);
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// GPUStatisticsCollector ========================================================================

GPUStatisticsCollector::GPUStatisticsCollector()
	: m_timestampsQueryPool(CreateQueryPool(rhi::EQueryType::Timestamp, 16384u))
	, m_timestampsQueryPoolIndex(0)
	, m_pipelineStatsQueryPool(CreateQueryPool(rhi::EQueryType::Statistics, 16384u, consts::queryFlags))
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
		const Uint32 statisticsBeginIndex = m_pipelineStatsQueryPoolIndex * consts::statisticsNum;
		recoder.BeginQuery(m_pipelineStatsQueryPool, m_pipelineStatsQueryPoolIndex);

		newScope.pipelineStatisticsQueryIdx = m_pipelineStatsQueryPoolIndex++;

		if (lib::HasAnyFlag(queryFlags, EQueryFlags::Rasterization))
		{
			newScope.fragmentShaderInvocationsIdx	= statisticsBeginIndex + 0u;
			newScope.taskShaderInvocationsIdx		= statisticsBeginIndex + 2u;
		}

		if (lib::HasAnyFlag(queryFlags, EQueryFlags::Compute))
		{
			newScope.computeShaderInvocationsIdx = statisticsBeginIndex + 1u;
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

void GPUStatisticsCollector::SetScopeStatisticsPSO(rdr::PipelineStateID psoID)
{
	SPT_CHECK(!m_scopesInProgressStack.empty());

	GPUStatisticsScopeDefinition& currentScope = *m_scopesInProgressStack.back();
	SPT_CHECK(!currentScope.statisticsPSO.IsValid());

	currentScope.statisticsPSO = psoID;
}

void GPUStatisticsCollector::SetScopeTransientMemoryUsage(Uint64 transientMemoryUsage)
{
	if (!m_collectMemoryDetails)
	{
		return;
	}

	SPT_CHECK(!m_scopesInProgressStack.empty());

	GPUStatisticsScopeDefinition& currentScope = *m_scopesInProgressStack.back();
	SPT_CHECK(!currentScope.transientMemoryUsage.has_value());

	currentScope.transientMemoryUsage = transientMemoryUsage;
}

void GPUStatisticsCollector::AddScopeTransientAllocation(const TransientAllocEventInfo& allocation)
{
	if (!m_collectMemoryDetails)
	{
		return;
	}

	m_transientAllocations.EmplaceBack(allocation);

	GPUStatisticsScopeDefinition& currentScope = *m_scopesInProgressStack.back();
	if (currentScope.transientAllocations.empty())
	{
		currentScope.transientAllocations = lib::Span<TransientAllocEventInfo>(m_transientAllocations.data() + m_transientAllocations.size() - 1u, 1u);
	}
	else
	{
		currentScope.transientAllocations = lib::Span<TransientAllocEventInfo>(currentScope.transientAllocations.data(), currentScope.transientAllocations.size() + 1u);
	}
}

void GPUStatisticsCollector::AddScopeTransientDeallocation(const TransientAllocEventInfo& deallocation)
{
	if (!m_collectMemoryDetails)
	{
		return;
	}

	m_transientDeallocations.EmplaceBack(deallocation);

	GPUStatisticsScopeDefinition& currentScope = *m_scopesInProgressStack.back();
	if (currentScope.transientDeallocations.empty())
	{
		currentScope.transientDeallocations = lib::Span<TransientAllocEventInfo>(m_transientDeallocations.data() + m_transientDeallocations.size() - 1u, 1u);
	}
	else
	{
		currentScope.transientDeallocations = lib::Span<TransientAllocEventInfo>(currentScope.transientDeallocations.data(), currentScope.transientDeallocations.size() + 1u);
	}
}

void GPUStatisticsCollector::CollectStatistics(OUT GPUStatisticsData& outStatistics)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(m_scopesInProgressStack.empty());

	outStatistics.Reset();

	StatisticsContext context;
	context.timestamps			= m_timestampsQueryPool->GetRHI().GetResults(m_timestampsQueryPoolIndex);
	context.pipelineStatistics	= m_pipelineStatsQueryPool->GetRHI().GetResults(m_pipelineStatsQueryPoolIndex);
	context.statisticsData		= &outStatistics;

	if (!context.timestamps.empty())
	{
		GPUStatisticsScopeData& frameStatistics = outStatistics.scopesPool.EmplaceBack();
		frameStatistics.name = lib::HashedString("GPU Frame");
		frameStatistics.children = outStatistics.scopesPool.EmplaceRange(m_scopeDefinitions.size());

		for (SizeType scopeIdx = 0u; scopeIdx < m_scopeDefinitions.size(); ++scopeIdx)
		{
			const GPUStatisticsScopeDefinition& scopeDef = m_scopeDefinitions[scopeIdx];
			GPUStatisticsScopeData& scopeData = frameStatistics.children[scopeIdx];

			scopeDef.EmitScopeResult(context, INOUT scopeData);
		}

		frameStatistics.beginTimestamp = std::min_element(std::cbegin(frameStatistics.children), std::cend(frameStatistics.children),
														  [](const GPUStatisticsScopeData& lhs, const GPUStatisticsScopeData& rhs)
														  {
															  return lhs.beginTimestamp < rhs.beginTimestamp;
														  })->beginTimestamp;

		frameStatistics.endTimestamp = std::max_element(std::cbegin(frameStatistics.children), std::cend(frameStatistics.children),
														[](const GPUStatisticsScopeData& lhs, const GPUStatisticsScopeData& rhs)
														{
															return lhs.endTimestamp < rhs.endTimestamp;
														})->endTimestamp;

		outStatistics.fullScope = &frameStatistics;
	}
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
