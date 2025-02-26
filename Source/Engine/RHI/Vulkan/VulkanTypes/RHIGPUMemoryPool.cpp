#include "RHIGPUMemoryPool.h"
#include "Vulkan/VulkanRHI.h"


namespace spt::vulkan
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// RHIGPUMemoryPoolReleaseTicket =================================================================

void RHIGPUMemoryPoolReleaseTicket::ExecuteReleaseRHI()
{
	if (handle.IsValid())
	{
		vmaFreeMemory(VulkanRHI::GetAllocatorHandle(), handle.GetValue());
		handle.Reset();
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// RHIGPUMemoryPool ==============================================================================

RHIGPUMemoryPool::RHIGPUMemoryPool()
	: m_allocation(VK_NULL_HANDLE)
{ }

void RHIGPUMemoryPool::InitializeRHI(const rhi::RHIMemoryPoolDefinition& definition, const rhi::RHIAllocationInfo& allocationInfo)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(!IsValid());

	// We don't need to set memory type mask - Vma will take care of it
	VkMemoryRequirements memoryRequirements{};
	memoryRequirements.alignment      = definition.alignment;
	memoryRequirements.size           = definition.size;
	memoryRequirements.memoryTypeBits = ~0u;

	const VmaAllocationCreateInfo vmaAllocationInfo = memory_utils::CreateAllocationInfo(allocationInfo);
	const VkResult allocResult = vmaAllocateMemory(VulkanRHI::GetAllocatorHandle(), &memoryRequirements, &vmaAllocationInfo, OUT & m_allocation, nullptr);
	SPT_CHECK_MSG(allocResult == VK_SUCCESS, "Failed to allocate memory for GPU memory pool");

	m_virtualAllocator.InitializeRHI(definition.size, definition.allocatorFlags);

	m_allocationInfo = allocationInfo;

	SPT_CHECK(IsValid());
}

void RHIGPUMemoryPool::ReleaseRHI()
{
	RHIGPUMemoryPoolReleaseTicket releaseTicket = DeferredReleaseRHI();
	releaseTicket.ExecuteReleaseRHI();
}

RHIGPUMemoryPoolReleaseTicket RHIGPUMemoryPool::DeferredReleaseRHI()
{
	SPT_CHECK(IsValid());

	RHIGPUMemoryPoolReleaseTicket releaseTicket;
	releaseTicket.handle = m_allocation;

#if SPT_RHI_DEBUG
	releaseTicket.name = GetName();
#endif SPT_RHI_DEBUG

	m_allocation = VK_NULL_HANDLE;

	SPT_CHECK(!IsValid());

	return releaseTicket;
}

Bool RHIGPUMemoryPool::IsValid() const
{
	return m_allocation != VK_NULL_HANDLE;
}

rhi::RHIVirtualAllocation RHIGPUMemoryPool::Allocate(const rhi::VirtualAllocationDefinition& definition)
{
	return m_virtualAllocator.Allocate(definition);
}

void RHIGPUMemoryPool::Free(const rhi::RHIVirtualAllocation& allocation)
{
	m_virtualAllocator.Free(allocation);
}

Uint64 RHIGPUMemoryPool::GetSize() const
{
	return m_virtualAllocator.GetSize();
}

void RHIGPUMemoryPool::SetName(const lib::HashedString& name)
{
	m_name.SetWithoutObject(name);

#if SPT_RHI_DEBUG
	if (m_allocation)
	{
		vmaSetAllocationName(VulkanRHI::GetAllocatorHandle(), m_allocation, name.GetData());
	}
#endif // SPT_RHI_DEBUG
}

const lib::HashedString& RHIGPUMemoryPool::GetName() const
{
	return m_name.Get();
}

VmaAllocation RHIGPUMemoryPool::GetAllocation() const
{
	return m_allocation;
}

rhi::RHIAllocationInfo RHIGPUMemoryPool::GetAllocationInfo() const
{
	SPT_CHECK(IsValid());

	return m_allocationInfo;
}

} // spt::vulkan
