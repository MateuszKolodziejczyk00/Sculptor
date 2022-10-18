#pragma once

#include "RHICommandPool.h"
#include "SculptorCoreTypes.h"
#include "RHICore/RHICommandBufferTypes.h"


namespace spt::vulkan
{

class CommandPoolsSet
{
public:

	CommandPoolsSet();
	~CommandPoolsSet();

	VkCommandBuffer AcquireCommandBuffer(const rhi::CommandBufferDefinition& cmdBufferDef);

	void			Reset();

private:

	RHICommandPool& AllocateNewPool(const rhi::CommandBufferDefinition& cmdBufferDef);

	lib::DynamicArray<RHICommandPool> m_pools;
};


class CommandPoolsLibrary
{
public:

	CommandPoolsLibrary();

	VkCommandBuffer		AcquireCommandBuffer(const rhi::CommandBufferDefinition& cmdBufferDef);

	void				ResetPools();
	
private:

	lib::HashMap<SizeType, CommandPoolsSet>	m_commandPools;
};


class CommandPoolsManager
{
public:

	CommandPoolsManager();

	void								DestroyResources();

	lib::UniquePtr<CommandPoolsLibrary>	AcquireCommandPoolsLibrary();
	void								ReleaseCommandPoolsLibrary(lib::UniquePtr<CommandPoolsLibrary> poolsLibrary);

private:

	lib::DynamicArray<lib::UniquePtr<CommandPoolsLibrary>> m_poolSets;
	lib::Lock m_lock;

};

} // spt::vulkan
