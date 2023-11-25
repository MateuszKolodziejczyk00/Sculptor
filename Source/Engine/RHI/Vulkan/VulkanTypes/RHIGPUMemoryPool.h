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

class RHI_API RHIGPUMemoryPool
{
public:

	RHIGPUMemoryPool();

	void InitializeRHI(const rhi::RHIMemoryPoolDefinition& definition, const rhi::RHIAllocationInfo& allocationInfo);
	void ReleaseRHI();

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
