#include "VulkanMemoryTypes.h"
#include "MathUtils.h"
#include "Vulkan/VulkanTypes/RHIGPUMemoryPool.h"
#include "Utility/Templates/Overload.h"


namespace spt::vulkan
{

namespace memory_utils
{

VmaVirtualAllocationCreateFlags GetVMAVirtualAllocationFlags(rhi::EVirtualAllocationFlags rhiFlags)
{
	VmaVirtualAllocationCreateFlags flags = 0;

	if (lib::HasAnyFlag(rhiFlags, rhi::EVirtualAllocationFlags::PreferUpperAddress))
	{
		lib::AddFlag(flags, VMA_VIRTUAL_ALLOCATION_CREATE_UPPER_ADDRESS_BIT);
	}
	if (lib::HasAnyFlag(rhiFlags, rhi::EVirtualAllocationFlags::PreferMinMemory))
	{
		lib::AddFlag(flags, VMA_VIRTUAL_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT);
	}
	if (lib::HasAnyFlag(rhiFlags, rhi::EVirtualAllocationFlags::PreferFasterAllocation))
	{
		lib::AddFlag(flags, VMA_VIRTUAL_ALLOCATION_CREATE_STRATEGY_MIN_TIME_BIT);
	}
	if (lib::HasAnyFlag(rhiFlags, rhi::EVirtualAllocationFlags::PreferMinOffset))
	{
		lib::AddFlag(flags, VMA_VIRTUAL_ALLOCATION_CREATE_STRATEGY_MIN_OFFSET_BIT);
	}

	return flags;
}

VmaMemoryUsage GetVMAMemoryUsage(rhi::EMemoryUsage memoryUsage)
{
	switch (memoryUsage)
	{
	case rhi::EMemoryUsage::GPUOnly:		return VMA_MEMORY_USAGE_GPU_ONLY;
	case rhi::EMemoryUsage::CPUOnly:		return VMA_MEMORY_USAGE_CPU_ONLY;
	case rhi::EMemoryUsage::CPUToGPU:		return VMA_MEMORY_USAGE_CPU_TO_GPU;
	case rhi::EMemoryUsage::GPUToCpu:		return VMA_MEMORY_USAGE_GPU_TO_CPU;
	case rhi::EMemoryUsage::CPUCopy:		return VMA_MEMORY_USAGE_CPU_COPY;
	}

	SPT_CHECK_NO_ENTRY();
	return VMA_MEMORY_USAGE_AUTO;
}

VmaAllocationCreateFlags GetVMAAllocationFlags(rhi::EAllocationFlags flags)
{
	VmaAllocationCreateFlags vmaFlags{};

	if (lib::HasAnyFlag(flags, rhi::EAllocationFlags::CreateDedicatedAllocation))
	{
		lib::AddFlag(vmaFlags, VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT);
	}
	if (lib::HasAnyFlag(flags, rhi::EAllocationFlags::CreateMapped))
	{
		lib::AddFlag(vmaFlags, VMA_ALLOCATION_CREATE_MAPPED_BIT);
	}

	return vmaFlags;
}

VmaVirtualBlockCreateFlags GetVMAVirtualBlockFlags(rhi::EVirtualAllocatorFlags flags)
{
	VmaVirtualBlockCreateFlags vmaFlags{};

	if (lib::HasAnyFlag(flags, rhi::EVirtualAllocatorFlags::Linear))
	{
		lib::AddFlag(vmaFlags, VMA_VIRTUAL_BLOCK_CREATE_LINEAR_ALGORITHM_BIT);
	}

	return vmaFlags;
}

VmaAllocationCreateInfo CreateAllocationInfo(const rhi::RHIAllocationInfo& rhiAllocationInfo)
{
	VmaAllocationCreateInfo allocationInfo{};
	allocationInfo.flags = GetVMAAllocationFlags(rhiAllocationInfo.allocationFlags);
	allocationInfo.usage = GetVMAMemoryUsage(rhiAllocationInfo.memoryUsage);

	return allocationInfo;
}

std::optional<rhi::RHIAllocationInfo> GetAllocationInfo(const rhi::RHIResourceAllocationDefinition& allocationDefinition)
{
	return std::visit(lib::Overload
					  {
						  [](const rhi::RHINullAllocationDefinition& ) -> std::optional<rhi::RHIAllocationInfo>
						  {
						  	return std::nullopt;
						  },
						  [](const rhi::RHICommittedAllocationDefinition& def) -> std::optional<rhi::RHIAllocationInfo>
						  {
						  	return def.allocationInfo;
						  },
						  [](const rhi::RHIPlacedAllocationDefinition& def) -> std::optional<rhi::RHIAllocationInfo>
						  {
						  	return def.pool->GetAllocationInfo();
						  }
					  },
					  allocationDefinition);
}

VmaAllocation GetVMAAllocation(const rhi::RHICommittedAllocation& allocation)
{
	return reinterpret_cast<VmaAllocation>(allocation.GetHandle());
}

VmaAllocation GetVMAAllocation(const rhi::RHIPlacedAllocation& allocation)
{
	return GetVMAAllocation(allocation.GetOwningAllocation());
}


VmaAllocation GetVMAAllocation(const rhi::RHIResourceAllocationHandle& allocation)
{
	return std::visit(lib::Overload
					  {
						  [](const rhi::RHINullAllocation&) -> VmaAllocation
						  {
						  	  return VK_NULL_HANDLE;
						  },
						  [](const rhi::RHIExternalAllocation&) -> VmaAllocation
						  {
						  	  return VK_NULL_HANDLE;
						  },
						  [](const auto& allocation) -> VmaAllocation
						  {
							  return GetVMAAllocation(allocation);
						  }
					  },
					  allocation);
}

} // memory_utils

VulkanVirtualAllocator::VulkanVirtualAllocator()
	: m_vitualBlock{}
	, m_size(0)
	, m_flags(rhi::EVirtualAllocatorFlags::None)
{ }

void VulkanVirtualAllocator::InitializeRHI(Uint64 memorySize, rhi::EVirtualAllocatorFlags flags /*= rhi::EVirtualAllocatorFlags::Default*/)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(!IsValid());

	VmaVirtualBlockCreateInfo blockInfo{};
	blockInfo.size = memorySize;
	SPT_VK_CHECK(vmaCreateVirtualBlock(&blockInfo, OUT &m_vitualBlock));

	m_size  = memorySize;
	m_flags = flags;
}

void VulkanVirtualAllocator::ReleaseRHI()
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(IsValid());

	if (lib::HasAnyFlag(m_flags, rhi::EVirtualAllocatorFlags::ClearOnRelease))
	{
		vmaClearVirtualBlock(m_vitualBlock);
	}

	vmaDestroyVirtualBlock(m_vitualBlock);

	m_vitualBlock = VK_NULL_HANDLE;
	m_size        = 0;
	m_flags       = rhi::EVirtualAllocatorFlags::None;
}

Bool VulkanVirtualAllocator::IsValid() const
{
	return m_vitualBlock != VK_NULL_HANDLE;
}

Uint64 VulkanVirtualAllocator::GetSize() const
{
	return m_size;
}

rhi::RHIVirtualAllocation VulkanVirtualAllocator::Allocate(const rhi::VirtualAllocationDefinition& definition)
{
	SPT_CHECK(definition.size > 0u);

	if (definition.size > GetSize())
	{
		return rhi::RHIVirtualAllocation{};
	}

	// alignment in VMA must be power of 2
	const Uint64 properAlignment      = math::Utils::IsPowerOf2(definition.alignment) ? definition.alignment : math::Utils::RoundDownToPowerOf2(definition.alignment);
	const Uint64 missingAlignment     = definition.alignment != properAlignment ? std::max<Uint64>(definition.alignment - properAlignment, properAlignment) : 0;
	const Uint64 properAllocationSize = definition.size + missingAlignment;

	VmaVirtualAllocationCreateInfo virtualAllocationDef{};
	virtualAllocationDef.alignment = properAlignment;
	virtualAllocationDef.size      = properAllocationSize;
	virtualAllocationDef.flags     = memory_utils::GetVMAVirtualAllocationFlags(definition.flags);

	VmaVirtualAllocation virtualAllocation = VK_NULL_HANDLE;
	VkDeviceSize offset = 0;
	const VkResult allocationResult = vmaVirtualAllocate(m_vitualBlock, &virtualAllocationDef, &virtualAllocation, &offset);

	SPT_CHECK(virtualAllocation != VK_NULL_HANDLE || allocationResult == VK_ERROR_OUT_OF_DEVICE_MEMORY);

	const Uint64 aligndDataOffset = offset == 0 ? offset : math::Utils::RoundUp(offset, definition.alignment);

	SPT_STATIC_CHECK(sizeof(VmaVirtualAllocation) == sizeof(Uint64));
	return allocationResult != VK_ERROR_OUT_OF_DEVICE_MEMORY ? rhi::RHIVirtualAllocation(reinterpret_cast<Uint64>(virtualAllocation), aligndDataOffset) : rhi::RHIVirtualAllocation();
}

void VulkanVirtualAllocator::Free(const rhi::RHIVirtualAllocation& allocation)
{
	SPT_CHECK(IsValid());
	SPT_CHECK(allocation.IsValid());

	vmaVirtualFree(m_vitualBlock, reinterpret_cast<VmaVirtualAllocation>(allocation.GetHandle()));
}

} // spt::vulkan
