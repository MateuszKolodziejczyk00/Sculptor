#include "RHICommandPoolsManager.h"
#include "Vulkan/VulkanRHI.h"
#include "Vulkan/Device/LogicalDevice.h"

namespace spt::vulkan
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// CommandPoolsSet ===============================================================================

CommandPoolsSet::CommandPoolsSet()
	: m_queueFamily(0)
	, m_flags(0)
{ }

CommandPoolsSet::~CommandPoolsSet()
{
	for (auto& cmdPool : m_commandPools)
	{
		cmdPool->ReleaseRHI();
	}

	m_commandPools.clear();
}

void CommandPoolsSet::Initialize(Uint32 queueFamilyIdx, VkCommandPoolCreateFlags flags, VkCommandBufferLevel level)
{
	// Initialization must happen before any pool is created
	SPT_CHECK(m_commandPools.empty());

	m_queueFamily	= queueFamilyIdx;
	m_flags			= flags;
	m_buffersLevel	= level;
}

VkCommandBuffer CommandPoolsSet::AcquireCommandBuffer(CommandBufferAcquireInfo& outAcquireInfo)
{
	SPT_PROFILE_FUNCTION();

	SizeType poolIdx = TryFindAndLockAvailablePool();
	if (poolIdx == m_commandPools.size())
	{
		poolIdx = CreateNewPool();
	}

	SPT_CHECK(m_commandPools[poolIdx]->IsLocked());

	outAcquireInfo.m_commandPoolId = poolIdx;

	return m_commandPools[poolIdx]->AcquireCommandBuffer();
}

void CommandPoolsSet::ReleasePool(const CommandBufferAcquireInfo& acquireInfo)
{
	SPT_PROFILE_FUNCTION();

	RHICommandPool& commandPool = SafeGetCommandPool(acquireInfo.m_commandPoolId);

	commandPool.Unlock();
}

void CommandPoolsSet::ReleaseCommandBuffer(const CommandBufferAcquireInfo& acquireInfo, VkCommandBuffer cmdBuffer)
{
	SPT_PROFILE_FUNCTION();

	const lib::WriteLockGuard lockGuard(m_lock);

	GetCommandPool_AssumesLocked(acquireInfo.m_commandPoolId).ReleaseCommandBuffer(cmdBuffer);
}

RHICommandPool& CommandPoolsSet::SafeGetCommandPool(SizeType commandPoolId)
{
	SPT_PROFILE_FUNCTION();

	const lib::ReadLockGuard lockGuard(m_lock);

	return *m_commandPools[commandPoolId];
}

RHICommandPool& CommandPoolsSet::GetCommandPool_AssumesLocked(SizeType commandPoolId)
{
	return *m_commandPools[commandPoolId];
}

SizeType CommandPoolsSet::TryFindAndLockAvailablePool()
{
	SPT_PROFILE_FUNCTION();

	const lib::ReadLockGuard lockGuard(m_lock);

	const auto foundAvailablePool = std::find_if(m_commandPools.begin(), m_commandPools.end(), [](const lib::UniquePtr<RHICommandPool>& pool) { return pool->TryLock(); });

	return std::distance(m_commandPools.begin(), foundAvailablePool);
}

SizeType CommandPoolsSet::CreateNewPool()
{
	SPT_PROFILE_FUNCTION();

	RHICommandPool* pool = nullptr;
	SizeType newPoolIdx = 0;

	{
		const lib::WriteLockGuard lockGuard(m_lock);

		pool = m_commandPools.emplace_back(std::make_unique<RHICommandPool>()).get();
		newPoolIdx = m_commandPools.size() - 1;

		pool->ForceLock();
	}

	SPT_CHECK(!!pool);
	pool->InitializeRHI(m_queueFamily, m_flags, m_buffersLevel);

	return newPoolIdx;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// CommandPoolsManager ===========================================================================

CommandPoolsManager::CommandPoolsManager()
{ }

void CommandPoolsManager::DestroyResources()
{
	m_poolSets.clear();
}

VkCommandBuffer CommandPoolsManager::AcquireCommandBuffer(const rhi::CommandBufferDefinition& bufferDefinition, CommandBufferAcquireInfo& outAcquireInfo)
{
	SPT_PROFILE_FUNCTION();

	const Uint32 poolSettingsHash = HashCommandBufferDefinition(bufferDefinition);
	
	outAcquireInfo.m_poolsSetHash = poolSettingsHash;

	CommandPoolsSet& poolsSet = GetPoolsSet(bufferDefinition, poolSettingsHash);

	const VkCommandBuffer cmdBufferHandle = poolsSet.AcquireCommandBuffer(outAcquireInfo);
	SPT_CHECK(cmdBufferHandle != VK_NULL_HANDLE && outAcquireInfo.m_commandPoolId != idxNone<Uint32>);

	return cmdBufferHandle;
}

void CommandPoolsManager::ReleasePool(const CommandBufferAcquireInfo& acquireInfo)
{
	SPT_PROFILE_FUNCTION();

	CommandPoolsSet& poolsSet = SafeGetPoolsSetByHash(acquireInfo.m_poolsSetHash);
	poolsSet.ReleasePool(acquireInfo);
}

void CommandPoolsManager::ReleaseCommandBuffer(const CommandBufferAcquireInfo& acquireInfo, VkCommandBuffer commandBuffer)
{
	SPT_PROFILE_FUNCTION();

	CommandPoolsSet& poolsSet = SafeGetPoolsSetByHash(acquireInfo.m_poolsSetHash);
	poolsSet.ReleaseCommandBuffer(acquireInfo, commandBuffer);
}

CommandPoolsSet& CommandPoolsManager::GetPoolsSet(const rhi::CommandBufferDefinition& bufferDefinition, Uint32 poolHash)
{
	SPT_PROFILE_FUNCTION();

	{
		const lib::ReadLockGuard lockGuard(m_lock);

		const auto poolSet = m_poolSets.find(poolHash);

		if (poolSet != m_poolSets.cend())
		{
			return *(poolSet->second);
		}
	}

	return CreatePoolSet(bufferDefinition, poolHash);
}

CommandPoolsSet& CommandPoolsManager::CreatePoolSet(const rhi::CommandBufferDefinition& bufferDefinition, Uint32 poolHash)
{
	SPT_PROFILE_FUNCTION();

	VkCommandPoolCreateFlags poolFlags = 0;
	if (bufferDefinition.m_complexityClass == rhi::ECommandBufferComplexityClass::Low)
	{
		poolFlags |= VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
	}

	const VkCommandBufferLevel level = (bufferDefinition.m_cmdBufferType == rhi::ECommandBufferType::Primary)
									 ? VK_COMMAND_BUFFER_LEVEL_PRIMARY
									 : VK_COMMAND_BUFFER_LEVEL_SECONDARY;

	lib::UniquePtr<CommandPoolsSet> commandPoolsSet = std::make_unique<CommandPoolsSet>();
	commandPoolsSet->Initialize(GetQueueFamilyIdx(bufferDefinition.m_queueType), poolFlags, level);

	const lib::WriteLockGuard lockGuard(m_lock);

	const lib::UniquePtr<CommandPoolsSet>& addedSet = m_poolSets[poolHash] = std::move(commandPoolsSet);
	return *addedSet;
}

CommandPoolsSet& CommandPoolsManager::SafeGetPoolsSetByHash(Uint32 poolHash)
{
	SPT_PROFILE_FUNCTION();

	const lib::ReadLockGuard lockGuard(m_lock);

	return *m_poolSets[poolHash];
}

Uint32 CommandPoolsManager::HashCommandBufferDefinition(const rhi::CommandBufferDefinition& bufferDefinition) const
{
	Uint32 result = 0;
	result += static_cast<Uint32>(bufferDefinition.m_queueType) << 5;
	result += static_cast<Uint32>(bufferDefinition.m_cmdBufferType) << 12;
	result += static_cast<Uint32>(bufferDefinition.m_complexityClass) << 23;

	return result;
}

Uint32 CommandPoolsManager::GetQueueFamilyIdx(rhi::ECommandBufferQueueType queueType) const
{
	const LogicalDevice& logicalDevice = VulkanRHI::GetLogicalDevice();

	return logicalDevice.GetQueueIdx(queueType);
}

}