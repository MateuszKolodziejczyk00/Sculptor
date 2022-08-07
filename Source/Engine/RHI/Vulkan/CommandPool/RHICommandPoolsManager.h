#pragma once

#include "RHICommandPool.h"
#include "SculptorCoreTypes.h"
#include "RHICore/RHICommandBufferTypes.h"
#include "RHICommandPoolsTypes.h"


namespace spt::vulkan
{

struct CommandPoolsSet
{
public:

	CommandPoolsSet();
	~CommandPoolsSet();

	void												Initialize(Uint32 queueFamilyIdx, VkCommandPoolCreateFlags flags, VkCommandBufferLevel level);

	VkCommandBuffer										AcquireCommandBuffer(CommandBufferAcquireInfo& outAcquireInfo);

	void												ReleasePool(const CommandBufferAcquireInfo& acquireInfo);

	void												ReleaseCommandBuffer(const CommandBufferAcquireInfo& acquireInfo, VkCommandBuffer cmdBuffer);
	
private:
	RHICommandPool&										SafeGetCommandPool(SizeType commandPoolId);
	RHICommandPool&										GetCommandPool_AssumesLocked(SizeType commandPoolId);

	SizeType											TryFindAndLockAvailablePool();

	SizeType											CreateNewPool();

	lib::DynamicArray<lib::UniquePtr<RHICommandPool>>	m_commandPools;
	lib::ReadWriteLock									m_lock;

	Uint32												m_queueFamily;
	VkCommandPoolCreateFlags							m_flags;
	VkCommandBufferLevel								m_buffersLevel;
};


class CommandPoolsManager
{
public:

	CommandPoolsManager();

	void												DestroyResources();

	VkCommandBuffer										AcquireCommandBuffer(const rhi::CommandBufferDefinition& bufferDefinition, CommandBufferAcquireInfo& outAcquireInfo);

	void												ReleasePool(const CommandBufferAcquireInfo& acquireInfo);

	void												ReleaseCommandBuffer(const CommandBufferAcquireInfo& acquireInfo, VkCommandBuffer commandBuffer);

private:

	CommandPoolsSet&									GetPoolsSet(const rhi::CommandBufferDefinition& bufferDefinition, Uint32 poolHash);
	CommandPoolsSet&									CreatePoolSet(const rhi::CommandBufferDefinition& bufferDefinition, Uint32 poolHash);

	CommandPoolsSet&									SafeGetPoolsSetByHash(Uint32 poolHash);

	Uint32												HashCommandBufferDefinition(const rhi::CommandBufferDefinition& bufferDefinition) const;

	Uint32												GetQueueFamilyIdx(rhi::ECommandBufferQueueType queueType) const;

	using CommandPoolsSetsMap							= lib::HashMap<Uint32, lib::UniquePtr<CommandPoolsSet>>;
	CommandPoolsSetsMap									m_poolSets;

	lib::ReadWriteLock									m_lock;
};

}
