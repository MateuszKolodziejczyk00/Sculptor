#include "RHIDescriptorHeap.h"
#include "Vulkan/VulkanRHI.h"
#include "Vulkan/Device/LogicalDevice.h"


namespace spt::vulkan
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// RHIDescriptorHeapReleaseTicket ================================================================

void RHIDescriptorHeapReleaseTicket::ExecuteReleaseRHI()
{
	bufferHandle.ExecuteReleaseRHI();
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// RHIDescriptorHeap =============================================================================

RHIDescriptorHeap::RHIDescriptorHeap()
{
}

void RHIDescriptorHeap::InitializeRHI(const rhi::DescriptorHeapDefinition& definition)
{
	SPT_PROFILER_FUNCTION();

	rhi::BufferDefinition bufferDef;
	bufferDef.size  = definition.size;
	bufferDef.usage = rhi::EBufferUsage::DescriptorBuffer;
	bufferDef.flags = rhi::EBufferFlags::WithVirtualSuballocations;

	rhi::RHICommittedAllocationDefinition allocationDef;
	allocationDef.allocationInfo.memoryUsage     = rhi::EMemoryUsage::CPUToGPU;
	allocationDef.allocationInfo.allocationFlags = rhi::EAllocationFlags::CreateMapped;
	allocationDef.alignment                      = VulkanRHI::GetLogicalDevice().GetDescriptorProps().descriptorsAlignment;
	m_buffer.InitializeRHI(bufferDef, allocationDef);
}

void RHIDescriptorHeap::ReleaseRHI()
{
	// nothing to do here
}

RHIDescriptorHeapReleaseTicket RHIDescriptorHeap::DeferredReleaseRHI()
{
	RHIBufferReleaseTicket bufferReleaseTicket = m_buffer.DeferredReleaseRHI();

	RHIDescriptorHeapReleaseTicket heapReleaseTicket;
	heapReleaseTicket.bufferHandle = std::move(bufferReleaseTicket);

	return heapReleaseTicket;
}

Bool RHIDescriptorHeap::IsValid() const
{
	return m_buffer.IsValid();
}

rhi::RHIDescriptorRange RHIDescriptorHeap::AllocateRange(Uint64 size)
{
	SPT_CHECK(IsValid());

	const lib::LockGuard lockGuard(m_lock);

	rhi::VirtualAllocationDefinition allocationDef;
	allocationDef.size      = size;
	allocationDef.alignment = VulkanRHI::GetLogicalDevice().GetDescriptorProps().descriptorsAlignment;
	const rhi::RHIVirtualAllocation allocation = m_buffer.CreateSuballocation(allocationDef);

	SPT_CHECK(allocation.IsValid());

	const rhi::RHIDescriptorRange range
	{
		.data             = lib::Span<Byte>{m_buffer.MapPtr() + allocation.GetOffset(), size},
		.allocationHandle = allocation.GetHandle(),
		.heapOffset       = static_cast<Uint32>(allocation.GetOffset()),
	};

	return range;
}

void RHIDescriptorHeap::DeallocateRange(rhi::RHIDescriptorRange range)
{
	const lib::LockGuard lockGuard(m_lock);

	m_buffer.DestroySuballocation(range.allocationHandle);
}

void RHIDescriptorHeap::SetName(const lib::HashedString& name)
{
	m_buffer.SetName(name);
}

const lib::HashedString& RHIDescriptorHeap::GetName() const
{
	return m_buffer.GetName();
}

} // spt::vulkan
