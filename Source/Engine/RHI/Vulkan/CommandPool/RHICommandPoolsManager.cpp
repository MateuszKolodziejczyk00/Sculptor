#include "RHICommandPoolsManager.h"
#include "Vulkan/VulkanRHI.h"
#include "Vulkan/Device/LogicalDevice.h"

namespace spt::vulkan
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// CommandPoolsSet ===============================================================================

CommandPoolsSet::CommandPoolsSet()
{ }

CommandPoolsSet::~CommandPoolsSet()
{
	for (RHICommandPool& cmdPool : m_pools)
	{
		cmdPool.ReleaseRHI();
	}
}

VkCommandBuffer CommandPoolsSet::AcquireCommandBuffer(const rhi::CommandBufferDefinition& cmdBufferDef)
{
	SPT_PROFILER_FUNCTION();

	for (RHICommandPool& cmdPool : m_pools)
	{
		const VkCommandBuffer cmdBuffer = cmdPool.TryAcquireCommandBuffer();

		if (cmdBuffer != VK_NULL_HANDLE)
		{
			return cmdBuffer;
		}
	}

	RHICommandPool& newPool = AllocateNewPool(cmdBufferDef);

	const VkCommandBuffer cmdBuffer = newPool.TryAcquireCommandBuffer();
	SPT_CHECK(cmdBuffer != VK_NULL_HANDLE);

	return cmdBuffer;
}

void CommandPoolsSet::Reset()
{
	SPT_PROFILER_FUNCTION();

	for (RHICommandPool& cmdPool : m_pools)
	{
		cmdPool.ResetCommandPool();
	}
}

RHICommandPool& CommandPoolsSet::AllocateNewPool(const rhi::CommandBufferDefinition& cmdBufferDef)
{
	SPT_PROFILER_FUNCTION();

	RHICommandPool& newPool = m_pools.emplace_back(RHICommandPool());

	const LogicalDevice& logicalDevice	= VulkanRHI::GetLogicalDevice();
	const Uint32 queueFamilyIdx			= logicalDevice.GetQueueFamilyIdx(cmdBufferDef.queueType);

	VkCommandPoolCreateFlags poolFlags = 0;
	if (cmdBufferDef.complexityClass == rhi::ECommandBufferComplexityClass::Low)
	{
		poolFlags |= VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
	}

	const VkCommandBufferLevel level = (cmdBufferDef.cmdBufferType == rhi::ECommandBufferType::Primary)
									 ? VK_COMMAND_BUFFER_LEVEL_PRIMARY
									 : VK_COMMAND_BUFFER_LEVEL_SECONDARY;
	newPool.InitializeRHI(queueFamilyIdx, poolFlags, level);

	return newPool;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// CommandPoolsCache =============================================================================

CommandPoolsLibrary::CommandPoolsLibrary()
{ }

VkCommandBuffer CommandPoolsLibrary::AcquireCommandBuffer(const rhi::CommandBufferDefinition& cmdBufferDef)
{
	SPT_PROFILER_FUNCTION();

	const SizeType definitionHash = lib::HashCombine(cmdBufferDef);
	return m_commandPools[definitionHash].AcquireCommandBuffer(cmdBufferDef);
}

void CommandPoolsLibrary::ResetPools()
{
	SPT_PROFILER_FUNCTION();

	for (auto& [hash, pool] : m_commandPools)
	{
		pool.Reset();
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// CommandPoolsCache =============================================================================

CommandPoolsManager::CommandPoolsManager()
{ }

void CommandPoolsManager::DestroyResources()
{
	m_poolSets.clear();
}

lib::UniquePtr<CommandPoolsLibrary> CommandPoolsManager::AcquireCommandPoolsLibrary()
{
	SPT_PROFILER_FUNCTION();

	{
		const lib::LockGuard lockGuard(m_lock);

		if (!m_poolSets.empty())
		{
			lib::UniquePtr<CommandPoolsLibrary> acquiredPool = std::move(m_poolSets.back());
			m_poolSets.pop_back();
			return acquiredPool;
		}
	}

	return std::make_unique<CommandPoolsLibrary>();
}

void CommandPoolsManager::ReleaseCommandPoolsLibrary(lib::UniquePtr<CommandPoolsLibrary> poolsLibrary)
{
	SPT_PROFILER_FUNCTION();

	poolsLibrary->ResetPools();

	const lib::LockGuard lockGuard(m_lock);

	m_poolSets.emplace_back(std::move(poolsLibrary));
}

} // spt::vulkan
