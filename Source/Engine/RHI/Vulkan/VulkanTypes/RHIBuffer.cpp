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

}

RHIBuffer::RHIBuffer()
	: m_bufferHandle(VK_NULL_HANDLE)
	, m_allocation(VK_NULL_HANDLE)
	, m_bufferSize(0)
	, m_usageFlags(rhi::EBufferUsage::None)
	, m_mappingStrategy(EMappingStrategy::CannotBeMapped)
	, m_mappedPointer(nullptr)
{ }

void RHIBuffer::InitializeRHI(Uint64 size, rhi::EBufferUsage bufferUsage, const rhi::RHIAllocationInfo& allocationInfo)
{
	SPT_PROFILE_FUNCTION();

	m_bufferSize = size;
	m_usageFlags = bufferUsage;

	const VmaAllocationCreateInfo vmaAllocationInfo = VulkanRHI::GetMemoryManager().CreateAllocationInfo(allocationInfo);

	const VkBufferUsageFlags vulkanUsage = priv::GetVulkanBufferUsage(bufferUsage);

	VkBufferCreateInfo bufferInfo{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    bufferInfo.flags = 0;
    bufferInfo.size = size;
    bufferInfo.usage = vulkanUsage;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	SPT_VK_CHECK(vmaCreateBuffer(VulkanRHI::GetAllocatorHandle(), &bufferInfo, &vmaAllocationInfo, &m_bufferHandle, &m_allocation, nullptr));

	InitializeMappingStrategy(vmaAllocationInfo);
}

void RHIBuffer::ReleaseRHI()
{
	SPT_PROFILE_FUNCTION();

	SPT_CHECK(IsValid());

	if (m_mappingStrategy == EMappingStrategy::PersistentlyMapped)
	{
		vmaUnmapMemory(VulkanRHI::GetAllocatorHandle(), m_allocation);
	}

	vmaDestroyBuffer(VulkanRHI::GetAllocatorHandle(), m_bufferHandle, m_allocation);

	m_bufferHandle = VK_NULL_HANDLE;
	m_allocation = VK_NULL_HANDLE;
	m_bufferSize = 0;
	m_usageFlags = rhi::EBufferUsage::None;
	m_name.Reset();
	m_mappingStrategy = EMappingStrategy::CannotBeMapped;
	m_mappedPointer = nullptr;
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

void* RHIBuffer::MapBufferMemory() const
{
	SPT_PROFILE_FUNCTION();

	if (m_mappingStrategy == EMappingStrategy::PersistentlyMapped)
	{
		SPT_CHECK(!!m_mappedPointer);
		return m_mappedPointer;
	}
	else if (m_mappingStrategy == EMappingStrategy::MappedWhenNecessary)
	{
		void* mappedPtr = nullptr;
		SPT_VK_CHECK(vmaMapMemory(VulkanRHI::GetAllocatorHandle(), m_allocation, &mappedPtr));
		return mappedPtr;
	}
	else
	{
		return nullptr;
	}
}

void RHIBuffer::UnmapBufferMemory() const
{
	SPT_PROFILE_FUNCTION();

	if (m_mappingStrategy == EMappingStrategy::MappedWhenNecessary)
	{
		vmaUnmapMemory(VulkanRHI::GetAllocatorHandle(), m_allocation);
	}
}

DeviceAddress RHIBuffer::GetDeviceAddress() const
{
	SPT_PROFILE_FUNCTION();

	VkBufferDeviceAddressInfo addressInfo{ VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO };
	addressInfo.buffer = m_bufferHandle;
	return vkGetBufferDeviceAddress(VulkanRHI::GetDeviceHandle(), &addressInfo);
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
		SPT_VK_CHECK(vmaMapMemory(VulkanRHI::GetAllocatorHandle(), m_allocation, &m_mappedPointer));
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

}
