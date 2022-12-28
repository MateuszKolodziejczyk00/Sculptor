#include "RHIBuffer.h"
#include "Vulkan/VulkanRHI.h"
#include "Vulkan/Memory/MemoryManager.h"

namespace spt::vulkan
{

namespace priv
{

VkBufferUsageFlags GetVulkanBufferUsage(rhi::EBufferUsage bufferUsage)
{
	VkBufferUsageFlags vulkanFlags{};

	if (lib::HasAnyFlag(bufferUsage, rhi::EBufferUsage::TransferSrc))
	{
		lib::AddFlag(vulkanFlags, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
	}
	if (lib::HasAnyFlag(bufferUsage, rhi::EBufferUsage::TransferDst))
	{
		lib::AddFlag(vulkanFlags, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	}
	if (lib::HasAnyFlag(bufferUsage, rhi::EBufferUsage::UniformTexel))
	{
		lib::AddFlag(vulkanFlags, VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT);
	}
	if (lib::HasAnyFlag(bufferUsage, rhi::EBufferUsage::StorageTexel))
	{
		lib::AddFlag(vulkanFlags, VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT);
	}
	if (lib::HasAnyFlag(bufferUsage, rhi::EBufferUsage::Uniform))
	{
		lib::AddFlag(vulkanFlags, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
	}
	if (lib::HasAnyFlag(bufferUsage, rhi::EBufferUsage::Storage))
	{
		lib::AddFlag(vulkanFlags, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	}
	if (lib::HasAnyFlag(bufferUsage, rhi::EBufferUsage::Index))
	{
		lib::AddFlag(vulkanFlags, VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
	}
	if (lib::HasAnyFlag(bufferUsage, rhi::EBufferUsage::Vertex))
	{
		lib::AddFlag(vulkanFlags, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
	}
	if (lib::HasAnyFlag(bufferUsage, rhi::EBufferUsage::Indirect))
	{
		lib::AddFlag(vulkanFlags, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT);
	}
	if (lib::HasAnyFlag(bufferUsage, rhi::EBufferUsage::DeviceAddress))
	{
		lib::AddFlag(vulkanFlags, VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
	}

	SPT_CHECK(static_cast<Flags32>(bufferUsage) < (static_cast<Flags32>(rhi::EBufferUsage::LAST) - 1) << 1);

	return vulkanFlags;
}

VmaVirtualAllocationCreateFlags GetVMAVirtualAllocationFlags(rhi::EBufferSuballocationFlags rhiFlags)
{
	VmaVirtualAllocationCreateFlags flags = 0;

	if (lib::HasAnyFlag(rhiFlags, rhi::EBufferSuballocationFlags::PreferUpperAddress))
	{
		lib::AddFlag(flags, VMA_VIRTUAL_ALLOCATION_CREATE_UPPER_ADDRESS_BIT);
	}
	if (lib::HasAnyFlag(rhiFlags, rhi::EBufferSuballocationFlags::PreferMinMemory))
	{
		lib::AddFlag(flags, VMA_VIRTUAL_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT);
	}
	if (lib::HasAnyFlag(rhiFlags, rhi::EBufferSuballocationFlags::PreferFasterAllocation))
	{
		lib::AddFlag(flags, VMA_VIRTUAL_ALLOCATION_CREATE_STRATEGY_MIN_TIME_BIT);
	}
	if (lib::HasAnyFlag(rhiFlags, rhi::EBufferSuballocationFlags::PreferMinOffset))
	{
		lib::AddFlag(flags, VMA_VIRTUAL_ALLOCATION_CREATE_STRATEGY_MIN_OFFSET_BIT);
	}

	return flags;
}

} // priv

RHIBuffer::RHIBuffer()
	: m_bufferHandle(VK_NULL_HANDLE)
	, m_allocation(VK_NULL_HANDLE)
	, m_bufferSize(0)
	, m_usageFlags(rhi::EBufferUsage::None)
	, m_mappingStrategy(EMappingStrategy::CannotBeMapped)
	, m_mappedPointer(nullptr)
	, m_allocatorVirtualBlock(VK_NULL_HANDLE)
{ }

void RHIBuffer::InitializeRHI(const rhi::BufferDefinition& definition, const rhi::RHIAllocationInfo& allocationInfo)
{
	SPT_PROFILER_FUNCTION();

	m_bufferSize = definition.size;
	m_usageFlags = definition.usage;

	const VmaAllocationCreateInfo vmaAllocationInfo = VulkanRHI::GetMemoryManager().CreateAllocationInfo(allocationInfo);

	const VkBufferUsageFlags vulkanUsage = priv::GetVulkanBufferUsage(definition.usage);

	VkBufferCreateInfo bufferInfo{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    bufferInfo.flags = 0;
    bufferInfo.size = definition.size;
    bufferInfo.usage = vulkanUsage;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	SPT_VK_CHECK(vmaCreateBuffer(VulkanRHI::GetAllocatorHandle(), &bufferInfo, &vmaAllocationInfo, &m_bufferHandle, &m_allocation, nullptr));

	InitializeMappingStrategy(vmaAllocationInfo);

	if (lib::HasAnyFlag(definition.flags, rhi::EBufferFlags::WithVirtualSuballocations))
	{
		InitVirtualBlock(definition);
	}
}

void RHIBuffer::ReleaseRHI()
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(IsValid());

	if (m_mappingStrategy == EMappingStrategy::PersistentlyMapped)
	{
		vmaUnmapMemory(VulkanRHI::GetAllocatorHandle(), m_allocation);
	}

	vmaDestroyBuffer(VulkanRHI::GetAllocatorHandle(), m_bufferHandle, m_allocation);

	if (m_allocatorVirtualBlock != VK_NULL_HANDLE)
	{
		vmaDestroyVirtualBlock(m_allocatorVirtualBlock);
	}

	m_bufferHandle = VK_NULL_HANDLE;
	m_allocation = VK_NULL_HANDLE;
	m_bufferSize = 0;
	m_usageFlags = rhi::EBufferUsage::None;
	m_name.Reset();
	m_mappingStrategy = EMappingStrategy::CannotBeMapped;
	m_mappedPointer = nullptr;
	m_allocatorVirtualBlock = VK_NULL_HANDLE;
}

Bool RHIBuffer::IsValid() const
{
	return m_bufferHandle != VK_NULL_HANDLE;
}

Uint64 RHIBuffer::GetSize() const
{
	return m_bufferSize;
}

rhi::EBufferUsage RHIBuffer::GetUsage() const
{
	return m_usageFlags;
}

Bool RHIBuffer::CanMapMemory() const
{
	return m_mappingStrategy != EMappingStrategy::CannotBeMapped;
}

Byte* RHIBuffer::MapBufferMemory() const
{
	SPT_PROFILER_FUNCTION();

	if (m_mappingStrategy == EMappingStrategy::PersistentlyMapped)
	{
		return m_mappedPointer;
	}
	else if (m_mappingStrategy == EMappingStrategy::MappedWhenNecessary)
	{
		void* mappedPtr = nullptr;
		SPT_VK_CHECK(vmaMapMemory(VulkanRHI::GetAllocatorHandle(), m_allocation, &mappedPtr));
		return static_cast<Byte*>(mappedPtr);
	}
	else
	{
		return nullptr;
	}
}

void RHIBuffer::UnmapBufferMemory() const
{
	SPT_PROFILER_FUNCTION();

	if (m_mappingStrategy == EMappingStrategy::MappedWhenNecessary)
	{
		vmaUnmapMemory(VulkanRHI::GetAllocatorHandle(), m_allocation);
	}
}

DeviceAddress RHIBuffer::GetDeviceAddress() const
{
	SPT_PROFILER_FUNCTION();

	VkBufferDeviceAddressInfo addressInfo{ VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO };
	addressInfo.buffer = m_bufferHandle;
	return vkGetBufferDeviceAddress(VulkanRHI::GetDeviceHandle(), &addressInfo);
}

Bool RHIBuffer::AllowsSuballocations() const
{
	return m_allocatorVirtualBlock != VK_NULL_HANDLE;
}

rhi::RHISuballocation RHIBuffer::CreateSuballocation(const rhi::SuballocationDefinition& definition)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(AllowsSuballocations());
	SPT_CHECK(definition.size > 0);

	VmaVirtualAllocationCreateInfo virtualAllocationDef{};
	virtualAllocationDef.alignment = definition.alignment;
	virtualAllocationDef.size = definition.size;
	virtualAllocationDef.flags = priv::GetVMAVirtualAllocationFlags(definition.flags);

	VmaVirtualAllocation virtualAllocation = VK_NULL_HANDLE;
	VkDeviceSize offset = 0;
	vmaVirtualAllocate(m_allocatorVirtualBlock, &virtualAllocationDef, &virtualAllocation, &offset);

	SPT_STATIC_CHECK(sizeof(VmaVirtualAllocation) == sizeof(Uint64));
	return virtualAllocation != VK_NULL_HANDLE ? rhi::RHISuballocation(reinterpret_cast<Uint64>(virtualAllocation), offset) : rhi::RHISuballocation();
}

void RHIBuffer::DestroySuballocation(rhi::RHISuballocation suballocation)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(AllowsSuballocations());
	SPT_CHECK(suballocation.IsValid());

	vmaVirtualFree(m_allocatorVirtualBlock, reinterpret_cast<VmaVirtualAllocation>(suballocation.GetSuballocationHandle()));
}

void RHIBuffer::SetName(const lib::HashedString& name)
{
	m_name.Set(name, reinterpret_cast<Uint64>(m_bufferHandle), VK_OBJECT_TYPE_BUFFER);
}

const lib::HashedString& RHIBuffer::GetName() const
{
	return m_name.Get();
}

VkBuffer RHIBuffer::GetBufferHandle() const
{
	return m_bufferHandle;
}

VmaAllocation RHIBuffer::GetAllocation() const
{
	return m_allocation;
}

void RHIBuffer::InitializeMappingStrategy(const VmaAllocationCreateInfo& allocationInfo)
{
	m_mappingStrategy = SelectMappingStrategy(allocationInfo);

	if (m_mappingStrategy == EMappingStrategy::PersistentlyMapped)
	{
		SPT_VK_CHECK(vmaMapMemory(VulkanRHI::GetAllocatorHandle(), m_allocation, reinterpret_cast<void**>(&m_mappedPointer)));
	}
}

RHIBuffer::EMappingStrategy RHIBuffer::SelectMappingStrategy(const VmaAllocationCreateInfo& allocationInfo) const
{
	const VmaMemoryUsage memoryUsage = allocationInfo.usage;

	// if allocation was created mapped, use persistently mapped strategy
	if(allocationInfo.flags &  VMA_ALLOCATION_CREATE_MAPPED_BIT)
	{
		return EMappingStrategy::PersistentlyMapped;
	}

	if (memoryUsage == VMA_MEMORY_USAGE_CPU_ONLY)
	{
		const Uint64 maxSizeForAlwaysPersistentlyMapped = 2048;

		// in case of CPU only buffers, we want them to be persistently mapped if they are small enough, otherwise mapped only when necessary
		return (m_bufferSize < maxSizeForAlwaysPersistentlyMapped) ? EMappingStrategy::PersistentlyMapped : EMappingStrategy::MappedWhenNecessary;
	}

	// All other host visible allocation should be mapped only when necessary
	if (memoryUsage == VMA_MEMORY_USAGE_CPU_TO_GPU || memoryUsage == VMA_MEMORY_USAGE_GPU_TO_CPU || memoryUsage == VMA_MEMORY_USAGE_CPU_COPY)
	{
		return EMappingStrategy::MappedWhenNecessary;
	}

	// Allocations that are not visible for host cannot be mapped
	return EMappingStrategy::CannotBeMapped;
}

void RHIBuffer::InitVirtualBlock(const rhi::BufferDefinition& definition)
{
	SPT_CHECK(lib::HasAnyFlag(definition.flags, rhi::EBufferFlags::WithVirtualSuballocations));

	VmaVirtualBlockCreateInfo blockInfo{};
	blockInfo.size = definition.size;
	vmaCreateVirtualBlock(&blockInfo, &m_allocatorVirtualBlock);
}

} // spt::vulkan
