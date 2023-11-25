#include "RHIBuffer.h"
#include "Vulkan/VulkanRHI.h"
#include "Vulkan/Memory/VulkanMemoryManager.h"
#include "MathUtils.h"
#include "Utility/Templates/Overload.h"
#include "RHIGPUMemoryPool.h"

namespace spt::vulkan
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// Priv ==========================================================================================

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
	if (lib::HasAnyFlag(bufferUsage, rhi::EBufferUsage::AccelerationStructureStorage))
	{
		lib::AddFlag(vulkanFlags, VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR);
	}
	if (lib::HasAnyFlag(bufferUsage, rhi::EBufferUsage::ASBuildInputReadOnly))
	{
		lib::AddFlag(vulkanFlags, VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR);
	}
	if (lib::HasAnyFlag(bufferUsage, rhi::EBufferUsage::ShaderBindingTable))
	{
		lib::AddFlag(vulkanFlags, VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR);
	}

	SPT_CHECK(static_cast<Flags32>(bufferUsage) < (static_cast<Flags32>(rhi::EBufferUsage::LAST) - 1) << 1);

	return vulkanFlags;
}

} // priv

//////////////////////////////////////////////////////////////////////////////////////////////////
// RHIMappedBufferBase ===========================================================================

RHIMappedByteBuffer::RHIMappedByteBuffer(const RHIBuffer& buffer)
	: m_buffer(buffer)
{
	m_mappedPointer = m_buffer.MapPtr();
	SPT_CHECK_MSG(!!m_mappedPointer, "Cannot Map buffer {0}", buffer.GetName().GetData());
}

RHIMappedByteBuffer::~RHIMappedByteBuffer()
{
	if (m_mappedPointer)
	{
		m_buffer.Unmap();
	}
}

RHIMappedByteBuffer::RHIMappedByteBuffer(RHIMappedByteBuffer&& other)
	: m_buffer(other.m_buffer)
	, m_mappedPointer(other.m_mappedPointer)
{
	other.m_mappedPointer = nullptr;
}

Byte* RHIMappedByteBuffer::GetPtr() const
{
	return m_mappedPointer;
}

Uint64 RHIMappedByteBuffer::GetSize() const
{
	return m_buffer.GetSize();
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// RHIBuffer =====================================================================================

RHIBuffer::RHIBuffer()
	: m_bufferHandle(VK_NULL_HANDLE)
	, m_bufferSize(0)
	, m_usageFlags(rhi::EBufferUsage::None)
	, m_mappingStrategy(EMappingStrategy::CannotBeMapped)
	, m_mappedPointer(nullptr)
{ }

void RHIBuffer::InitializeRHI(const rhi::BufferDefinition& definition, const rhi::RHIResourceAllocationDefinition& allocationDef)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK_MSG(definition.size > 0, "Buffer size must be greater than 0");

	m_bufferSize = definition.size;
	m_usageFlags = definition.usage;

	const VkBufferUsageFlags vulkanUsage = priv::GetVulkanBufferUsage(definition.usage);

	VkBufferCreateInfo bufferInfo{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    bufferInfo.flags = 0;
    bufferInfo.size = definition.size;
    bufferInfo.usage = vulkanUsage;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	SPT_VK_CHECK(vkCreateBuffer(VulkanRHI::GetDeviceHandle(), &bufferInfo, VulkanRHI::GetAllocationCallbacks(), OUT &m_bufferHandle));

	BindMemory(allocationDef);

	// Allocator lifetime is always same as buffer - so create it even if memory is not bound yet
	if (lib::HasAnyFlag(definition.flags, rhi::EBufferFlags::WithVirtualSuballocations))
	{
		m_virtualAllocator.InitializeRHI(definition.size, rhi::EVirtualAllocatorFlags::ClearOnRelease);
	}
}

void RHIBuffer::ReleaseRHI()
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(IsValid());

	SPT_CHECK_MSG(!std::holds_alternative<rhi::RHIExternalAllocation>(m_allocationHandle), "Buffers cannot be externally allocated!");
	SPT_CHECK_MSG(!std::holds_alternative<rhi::RHIPlacedAllocation>(m_allocationHandle), "Placed allocations must be released manually before releasing resource!");

	VmaAllocation allocationToRelease = VK_NULL_HANDLE;

	if (std::holds_alternative<rhi::RHICommittedAllocation>(m_allocationHandle))
	{
		const rhi::RHICommittedAllocation& committedAllocation = std::get<rhi::RHICommittedAllocation>(m_allocationHandle);
		allocationToRelease = memory_utils::GetVMAAllocation(committedAllocation);
	}

	if (allocationToRelease)
	{
		PreUnbindMemory(allocationToRelease);
	}

	SPT_CHECK(!m_mappedPointer);

	if (m_virtualAllocator.IsValid())
	{
		m_virtualAllocator.ReleaseRHI();
	}
	
	m_name.Reset(reinterpret_cast<Uint64>(m_bufferHandle), VK_OBJECT_TYPE_BUFFER);

	vkDestroyBuffer(VulkanRHI::GetDeviceHandle(), m_bufferHandle, VulkanRHI::GetAllocationCallbacks());

	if (allocationToRelease != VK_NULL_HANDLE)
	{
		vmaFreeMemory(VulkanRHI::GetAllocatorHandle(), allocationToRelease);
	}

	m_bufferHandle     = VK_NULL_HANDLE;
	m_allocationHandle = rhi::RHINullAllocation{};
	m_bufferSize       = 0;
	m_usageFlags       = rhi::EBufferUsage::None;
	m_mappingStrategy  = EMappingStrategy::CannotBeMapped;
	m_mappedPointer    = nullptr;
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

Bool RHIBuffer::HasBoundMemory() const
{
	return !std::holds_alternative<rhi::RHINullAllocation>(m_allocationHandle);
}

Bool RHIBuffer::CanMapMemory() const
{
	return m_mappingStrategy != EMappingStrategy::CannotBeMapped;
}

Byte* RHIBuffer::MapPtr() const
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(HasBoundMemory());

	if (m_mappingStrategy == EMappingStrategy::PersistentlyMapped)
	{
		return m_mappedPointer;
	}
	else if (m_mappingStrategy == EMappingStrategy::MappedWhenNecessary)
	{
		void* mappedPtr = nullptr;
		
		SPT_VK_CHECK(vmaMapMemory(VulkanRHI::GetAllocatorHandle(), GetAllocation(), &mappedPtr));
		return static_cast<Byte*>(mappedPtr);
	}
	else
	{
		return nullptr;
	}
}

void RHIBuffer::Unmap() const
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(HasBoundMemory());

	if (m_mappingStrategy == EMappingStrategy::MappedWhenNecessary)
	{
		vmaUnmapMemory(VulkanRHI::GetAllocatorHandle(), GetAllocation());
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
	return m_virtualAllocator.IsValid();
}

rhi::RHIVirtualAllocation RHIBuffer::CreateSuballocation(const rhi::VirtualAllocationDefinition& definition)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(AllowsSuballocations());

	return m_virtualAllocator.Allocate(definition);
}

void RHIBuffer::DestroySuballocation(rhi::RHIVirtualAllocation suballocation)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(AllowsSuballocations());

	m_virtualAllocator.Free(suballocation);
}

rhi::RHIMemoryRequirements RHIBuffer::GetMemoryRequirements() const
{
	SPT_CHECK(IsValid());

	rhi::RHIMemoryRequirements requirements;

	VkMemoryRequirements vkRequirements;
	vkGetBufferMemoryRequirements(VulkanRHI::GetDeviceHandle(), m_bufferHandle, OUT &vkRequirements);

	requirements.alignment = vkRequirements.alignment;
	requirements.size      = vkRequirements.size;

	return requirements;
}

void RHIBuffer::SetName(const lib::HashedString& name)
{
	m_name.Set(name, reinterpret_cast<Uint64>(m_bufferHandle), VK_OBJECT_TYPE_BUFFER);
	
#if RHI_DEBUG
	if (std::holds_alternative<rhi::RHICommittedAllocation>(m_allocationHandle))
	{
		const VmaAllocation allocation = memory_utils::GetVMAAllocation(std::get<rhi::RHICommittedAllocation>(m_allocationHandle));
		vmaSetAllocationName(VulkanRHI::GetAllocatorHandle(), allocation, name.GetData());
	}
#endif // RHI_DEBUG
}

const lib::HashedString& RHIBuffer::GetName() const
{
	return m_name.Get();
}

VkBuffer RHIBuffer::GetHandle() const
{
	return m_bufferHandle;
}

Bool RHIBuffer::BindMemory(const rhi::RHIResourceAllocationDefinition& allocationDefinition)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(IsValid());
	SPT_CHECK(!HasBoundMemory());

	m_allocationHandle = std::visit(lib::Overload
									{
										[&](const rhi::RHINullAllocationDefinition& nullAllocation) -> rhi::RHIResourceAllocationHandle
										{
											return rhi::RHINullAllocation{};
										},
										[this](const rhi::RHIPlacedAllocationDefinition& placedAllocation) -> rhi::RHIResourceAllocationHandle
										{
											return DoPlacedAllocation(placedAllocation);
										},
										[&](const rhi::RHICommittedAllocationDefinition& committedAllocation) -> rhi::RHIResourceAllocationHandle
										{
											return DoCommittedAllocation(committedAllocation);
										}
									},
									allocationDefinition);

	const Bool success = HasBoundMemory();

	if (success)
	{
		const std::optional<rhi::RHIAllocationInfo> allocationInfo = memory_utils::GetAllocationInfo(allocationDefinition);
		SPT_CHECK(allocationInfo.has_value());
		InitializeMappingStrategy(*allocationInfo);
	}

	return success;
}

rhi::RHIResourceAllocationHandle RHIBuffer::ReleasePlacedAllocation()
{
	SPT_CHECK(IsValid());
	SPT_CHECK(std::holds_alternative<rhi::RHIPlacedAllocation>(m_allocationHandle));

	const rhi::RHIPlacedAllocation allocation = std::get<rhi::RHIPlacedAllocation>(m_allocationHandle);

	PreUnbindMemory(memory_utils::GetVMAAllocation(allocation));

	m_allocationHandle = rhi::RHINullAllocation{};

	return allocation;
}

rhi::RHIResourceAllocationHandle RHIBuffer::DoPlacedAllocation(const rhi::RHIPlacedAllocationDefinition& placedAllocationDef)
{
	SPT_PROFILER_FUNCTION();
	
	SPT_CHECK(!!placedAllocationDef.pool);
	SPT_CHECK(placedAllocationDef.pool->IsValid());

	VkMemoryRequirements memoryRequirements{};
	vkGetBufferMemoryRequirements(VulkanRHI::GetDeviceHandle(), m_bufferHandle, OUT &memoryRequirements);

	rhi::VirtualAllocationDefinition suballocationDefinition{};
	suballocationDefinition.size      = memoryRequirements.size;
	suballocationDefinition.alignment = memoryRequirements.alignment;
	suballocationDefinition.flags     = placedAllocationDef.flags;

	const rhi::RHIVirtualAllocation suballocation = placedAllocationDef.pool->Allocate(suballocationDefinition);
	if (!suballocation.IsValid())
	{
		return rhi::RHINullAllocation{};
	}

	const VmaAllocation poolMemoryAllocation = placedAllocationDef.pool->GetAllocation();

	vmaBindBufferMemory2(VulkanRHI::GetAllocatorHandle(), poolMemoryAllocation, suballocation.GetOffset(), m_bufferHandle, nullptr);

	return rhi::RHIPlacedAllocation(rhi::RHICommittedAllocation(reinterpret_cast<Uint64>(poolMemoryAllocation)), suballocation);
}

rhi::RHIResourceAllocationHandle RHIBuffer::DoCommittedAllocation(const rhi::RHICommittedAllocationDefinition& committedAllocation)
{
	SPT_PROFILER_FUNCTION();

	VmaAllocation allocation = VK_NULL_HANDLE;

	VmaAllocationCreateInfo allocationInfo{};
	allocationInfo.flags = memory_utils::GetVMAAllocationFlags(committedAllocation.allocationInfo.allocationFlags);
	allocationInfo.usage = memory_utils::GetVMAMemoryUsage(committedAllocation.allocationInfo.memoryUsage);
	SPT_VK_CHECK(vmaAllocateMemoryForBuffer(VulkanRHI::GetAllocatorHandle(), m_bufferHandle, &allocationInfo, OUT &allocation, nullptr));

	SPT_CHECK(allocation != VK_NULL_HANDLE);

	vmaBindBufferMemory2(VulkanRHI::GetAllocatorHandle(), allocation, 0, m_bufferHandle, nullptr);

#if RHI_DEBUG
	if (m_name.HasName())
	{
		vmaSetAllocationName(VulkanRHI::GetAllocatorHandle(), allocation, m_name.Get().GetData());
	}
#endif // RHI_DEBUG

	return rhi::RHICommittedAllocation(reinterpret_cast<Uint64>(allocation));
}

void RHIBuffer::PreUnbindMemory(VmaAllocation allocation)
{
	SPT_CHECK(IsValid());
	SPT_CHECK(HasBoundMemory());

	if (m_mappingStrategy == EMappingStrategy::PersistentlyMapped)
	{
		vmaUnmapMemory(VulkanRHI::GetAllocatorHandle(), allocation);
		m_mappedPointer = nullptr;
	}
}

VmaAllocation RHIBuffer::GetAllocation() const
{
	return memory_utils::GetVMAAllocation(m_allocationHandle);
}

void RHIBuffer::InitializeMappingStrategy(const rhi::RHIAllocationInfo& allocationInfo)
{
	SPT_CHECK(HasBoundMemory());

	m_mappingStrategy = SelectMappingStrategy(allocationInfo);

	if (m_mappingStrategy == EMappingStrategy::PersistentlyMapped)
	{
		const VmaAllocation allocation = memory_utils::GetVMAAllocation(m_allocationHandle);
		SPT_CHECK(allocation != VK_NULL_HANDLE);
		SPT_VK_CHECK(vmaMapMemory(VulkanRHI::GetAllocatorHandle(), allocation, reinterpret_cast<void**>(&m_mappedPointer)));
	}
}

RHIBuffer::EMappingStrategy RHIBuffer::SelectMappingStrategy(const rhi::RHIAllocationInfo& allocationInfo) const
{
	// if allocation was created mapped, use persistently mapped strategy
	if(lib::HasAnyFlag(allocationInfo.allocationFlags, rhi::EAllocationFlags::CreateMapped))
	{
		return EMappingStrategy::PersistentlyMapped;
	}

	const rhi::EMemoryUsage memoryUsage = allocationInfo.memoryUsage;

	if (memoryUsage == rhi::EMemoryUsage::CPUOnly)
	{
		constexpr Uint64 maxSizeForAlwaysPersistentlyMapped = 2048;

		// in case of CPU only buffers, we want them to be persistently mapped if they are small enough, otherwise mapped only when necessary
		return (m_bufferSize < maxSizeForAlwaysPersistentlyMapped) ? EMappingStrategy::PersistentlyMapped : EMappingStrategy::MappedWhenNecessary;
	}

	// All other host visible allocation should be mapped only when necessary
	if (memoryUsage == rhi::EMemoryUsage::CPUToGPU || memoryUsage == rhi::EMemoryUsage::GPUToCpu || memoryUsage == rhi::EMemoryUsage::CPUCopy)
	{
		return EMappingStrategy::MappedWhenNecessary;
	}

	// Allocations that are not visible for host cannot be mapped
	return EMappingStrategy::CannotBeMapped;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// RHIBufferMemoryOwner ==========================================================================

Bool RHIBufferMemoryOwner::BindMemory(RHIBuffer& buffer, const rhi::RHIResourceAllocationDefinition& allocationDefinition)
{
	return buffer.BindMemory(allocationDefinition);
}

rhi::RHIResourceAllocationHandle RHIBufferMemoryOwner::ReleasePlacedAllocation(RHIBuffer& buffer)
{
	return buffer.ReleasePlacedAllocation();
}

} // spt::vulkan
