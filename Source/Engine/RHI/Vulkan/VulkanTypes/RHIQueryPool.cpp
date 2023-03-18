#include "RHIQueryPool.h"
#include "Vulkan/VulkanRHIUtils.h"
#include "Vulkan/VulkanRHI.h"

namespace spt::vulkan
{

RHIQueryPool::RHIQueryPool()
	: m_handle(VK_NULL_HANDLE)
	, m_queryCount(0)
{ }

void RHIQueryPool::InitializeRHI(const rhi::QueryPoolDefinition& definition)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(!IsValid());

	VkQueryPoolCreateInfo queryPoolInfo{ VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO };
	queryPoolInfo.queryType		= RHIToVulkan::GetQueryType(definition.queryType);
	queryPoolInfo.queryCount	= definition.queryCount;

	SPT_VK_CHECK(vkCreateQueryPool(VulkanRHI::GetDeviceHandle(), &queryPoolInfo, VulkanRHI::GetAllocationCallbacks(), &m_handle));

	m_queryCount = definition.queryCount;
}

void RHIQueryPool::ReleaseRHI()
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(IsValid());

	vkDestroyQueryPool(VulkanRHI::GetDeviceHandle(), m_handle, VulkanRHI::GetAllocationCallbacks());
	m_handle		= VK_NULL_HANDLE;
	m_queryCount	= 0;
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
	return m_queryCount;
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

	lib::DynamicArray<Uint64> results;
	results.resize(queryCount);

	const VkResult result = vkGetQueryPoolResults(VulkanRHI::GetDeviceHandle(), m_handle, 0, queryCount, results.size() * sizeof(Uint64), results.data(), sizeof(Uint64), VK_QUERY_RESULT_64_BIT);

	return result == VK_SUCCESS ? results : lib::DynamicArray<Uint64>();
}

} // spt::vulkan
