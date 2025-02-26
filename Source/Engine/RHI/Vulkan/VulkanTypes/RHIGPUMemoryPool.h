#pragma once

#include "RHIMacros.h"
#include "SculptorCoreTypes.h"
#include "RHICore/RHIBufferTypes.h"
#include "Vulkan/VulkanCore.h"
#include "Vulkan/Debug/DebugUtils.h"
#include "Vulkan/Memory/VulkanMemoryTypes.h"


namespace spt::rhi
{
struct RHIAllocationInfo;
}


namespace spt::vulkan
{

struct RHI_API RHIGPUMemoryPoolReleaseTicket
{
	void ExecuteReleaseRHI();

	RHIResourceReleaseTicket<VmaAllocation> handle;

#if SPT_RHI_DEBUG
	lib::HashedString name;
#endif // SPT_RHI_DEBUG
};


class RHI_API RHIGPUMemoryPool
{
public:

	RHIGPUMemoryPool();

	void InitializeRHI(const rhi::RHIMemoryPoolDefinition& definition, const rhi::RHIAllocationInfo& allocationInfo);
	void ReleaseRHI();

	RHIGPUMemoryPoolReleaseTicket DeferredReleaseRHI();

	Bool IsValid() const;

	rhi::RHIVirtualAllocation Allocate(const rhi::VirtualAllocationDefinition& definition);
	void Free(const rhi::RHIVirtualAllocation& allocation);

	Uint64 GetSize() const;

	void                     SetName(const lib::HashedString& name);
	const lib::HashedString& GetName() const;

	// Vulkan =========================================================================================================

	VmaAllocation GetAllocation() const;

	rhi::RHIAllocationInfo GetAllocationInfo() const;

private:

	VmaAllocation m_allocation;

	VulkanVirtualAllocator m_virtualAllocator;

	rhi::RHIAllocationInfo m_allocationInfo;

	DebugName m_name;
};

} // spt::vulkan
