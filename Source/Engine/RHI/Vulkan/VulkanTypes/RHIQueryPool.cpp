#include "RHIQueryPool.h"
#include "Vulkan/VulkanRHIUtils.h"
#include "Vulkan/VulkanRHI.h"
#include "MathUtils.h"

namespace spt::vulkan
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// RHIQueryPoolReleaseTicket =====================================================================

void RHIQueryPoolReleaseTicket::ExecuteReleaseRHI()
{
	if (handle.IsValid())
	{
		vkDestroyQueryPool(VulkanRHI::GetDeviceHandle(), handle.GetValue(), VulkanRHI::GetAllocationCallbacks());
		handle.Reset();
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// RHIQueryPool ===================================================================================

RHIQueryPool::RHIQueryPool()
	: m_handle(VK_NULL_HANDLE)
{ }

void RHIQueryPool::InitializeRHI(const rhi::QueryPoolDefinition& definition)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(!IsValid());

	VkQueryPoolCreateInfo queryPoolInfo{ VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO };
	queryPoolInfo.queryCount			= definition.queryCount;
	queryPoolInfo.queryType				= RHIToVulkan::GetQueryType(definition.queryType);
	queryPoolInfo.pipelineStatistics	= RHIToVulkan::GetPipelineStatistic(definition.statisticsType);

	SPT_VK_CHECK(vkCreateQueryPool(VulkanRHI::GetDeviceHandle(), &queryPoolInfo, VulkanRHI::GetAllocationCallbacks(), &m_handle));

	m_definition = definition;

	SPT_CHECK(IsValid());
}

void RHIQueryPool::ReleaseRHI()
{
	RHIQueryPoolReleaseTicket releaseTicket = DeferredReleaseRHI();
	releaseTicket.ExecuteReleaseRHI();
}

RHIQueryPoolReleaseTicket RHIQueryPool::DeferredReleaseRHI()
{
	SPT_CHECK(IsValid());

	RHIQueryPoolReleaseTicket releaseTicket;
	releaseTicket.handle = m_handle;

	m_handle = VK_NULL_HANDLE;

	SPT_CHECK(!IsValid());

	return releaseTicket;
}

Bool RHIQueryPool::IsValid() const
{
	return m_handle != VK_NULL_HANDLE;
}

VkQueryPool RHIQueryPool::GetHandle() const
{
	return m_handle;
}

Uint32 RHIQueryPool::GetQueryCount() const
{
	return m_definition.queryCount;
}

void RHIQueryPool::Reset(Uint32 firstQuery, Uint32 queryCount)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(IsValid());

	vkResetQueryPool(VulkanRHI::GetDeviceHandle(), m_handle, firstQuery, queryCount);
}

lib::DynamicArray<Uint64> RHIQueryPool::GetResults(Uint32 queryCount) const
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(IsValid());

	if (queryCount == 0)
	{
		return lib::DynamicArray<Uint64>();
	}

	const SizeType valuesPerQuery = m_definition.queryType == rhi::EQueryType::Statistics ? static_cast<Uint64>(math::Utils::CountSetBits(static_cast<Uint32>(m_definition.statisticsType))) : 1u;
	const SizeType resultsSize = queryCount * valuesPerQuery;

	lib::DynamicArray<Uint64> results;
	results.resize(resultsSize);

	const VkResult result = vkGetQueryPoolResults(VulkanRHI::GetDeviceHandle(), m_handle, 0, queryCount, results.size() * sizeof(Uint64), results.data(), valuesPerQuery * sizeof(Uint64), VK_QUERY_RESULT_64_BIT);

	return result == VK_SUCCESS ? results : lib::DynamicArray<Uint64>();
}

} // spt::vulkan
