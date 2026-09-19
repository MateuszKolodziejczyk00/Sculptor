#include "RHIDescriptorHeap.h"
#include "Vulkan/VulkanRHI.h"
#include "Vulkan/Device/LogicalDevice.h"
#include "Vulkan/VulkanRHIUtils.h"


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

	const rhi::DescriptorProps& descriptorProps = VulkanRHI::GetLogicalDevice().GetDescriptorProps();

	rhi::BufferDefinition bufferDef;
	bufferDef.size  = definition.size + (definition.type == rhi::EDescriptorHeapType::Sampler ? descriptorProps.reservedSamplerHeapSize : descriptorProps.reservedResourceHeapSize);
	bufferDef.usage = definition.type == rhi::EDescriptorHeapType::Sampler ? rhi::EBufferUsage::SamplerDescriptorHeap : rhi::EBufferUsage::ResourceDescriptorHeap;
	bufferDef.flags = rhi::EBufferFlags::WithVirtualSuballocations;

	rhi::RHICommittedAllocationDefinition allocationDef;
	allocationDef.allocationInfo.memoryUsage     = rhi::EMemoryUsage::CPUToGPU;
	allocationDef.allocationInfo.allocationFlags = rhi::EAllocationFlags::CreateMapped;
	allocationDef.alignment                      = VulkanRHI::GetLogicalDevice().GetDescriptorProps().descriptorsAlignment;
	m_buffer.InitializeRHI(bufferDef, allocationDef);

	m_descriptorSize = definition.type == rhi::EDescriptorHeapType::Sampler ? descriptorProps.samplerDescriptorSize : descriptorProps.resourceDescriptorSize;
	m_descriptorsNum = static_cast<Uint32>(definition.size / m_descriptorSize);

	m_mappedBuffer.Construct(m_buffer);
}

void RHIDescriptorHeap::ReleaseRHI()
{
	m_mappedBuffer.Destroy();
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

rhi::EDescriptorHeapType RHIDescriptorHeap::GetType() const
{
	SPT_CHECK(IsValid());

	return lib::HasAnyFlag(m_buffer.GetUsage(), rhi::EBufferUsage::SamplerDescriptorHeap) ? rhi::EDescriptorHeapType::Sampler : rhi::EDescriptorHeapType::Resource;
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

void RHIDescriptorHeap::CopySamplerDescriptor(const rhi::SamplerDefinition& def, lib::Span<Byte> dst)
{
	VkSamplerCreateInfo samplerInfo{ VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
	samplerInfo.flags                      = RHIToVulkan::GetSamplerCreateFlags(def.flags);
	samplerInfo.magFilter                  = RHIToVulkan::GetSamplerFilterType(def.magnificationFilter);
	samplerInfo.minFilter                  = RHIToVulkan::GetSamplerFilterType(def.minificationFilter);
	samplerInfo.mipmapMode                 = RHIToVulkan::GetMipMapAddressingMode(def.mipMapAdressingMode);
	samplerInfo.addressModeU               = RHIToVulkan::GetAxisAddressingMode(def.addressingModeU);
	samplerInfo.addressModeV               = RHIToVulkan::GetAxisAddressingMode(def.addressingModeV);
	samplerInfo.addressModeW               = RHIToVulkan::GetAxisAddressingMode(def.addressingModeW);
	samplerInfo.mipLodBias                 = def.mipLodBias;
	samplerInfo.anisotropyEnable           = def.enableAnisotropy;
	samplerInfo.maxAnisotropy              = def.maxAnisotropy;
	samplerInfo.minLod                     = def.minLod;
	samplerInfo.maxLod                     = def.maxLod < 0.f ? VK_LOD_CLAMP_NONE : def.maxLod;
	samplerInfo.borderColor                = RHIToVulkan::GetBorderColor(def.borderColor);
	samplerInfo.unnormalizedCoordinates    = def.unnormalizedCoords;
	
	if (def.compareOp != rhi::ECompareOp::None)
	{
		samplerInfo.compareEnable   = VK_TRUE;
		samplerInfo.compareOp       = RHIToVulkan::GetCompareOp(def.compareOp);
	}
	
	VkSamplerReductionModeCreateInfo reductionModeInfo{ VK_STRUCTURE_TYPE_SAMPLER_REDUCTION_MODE_CREATE_INFO };
	reductionModeInfo.reductionMode = RHIToVulkan::GetSamplerReductionMode(def.reductionMode);
	
	samplerInfo.pNext = &reductionModeInfo;

	VkHostAddressRangeEXT hostAddressRange;
	hostAddressRange.address = dst.data();
	hostAddressRange.size    = dst.size();

	vkWriteSamplerDescriptorsEXT(VulkanRHI::GetDeviceHandle(), 1u, &samplerInfo, &hostAddressRange);
}

} // spt::vulkan
