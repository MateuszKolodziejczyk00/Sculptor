#include "RHIBuffer.h"
#include "VulkanRHI.h"

namespace spt::vulkan
{

RHIBuffer::RHIBuffer(VmaAllocator allocator, Uint64 size, Flags32 bufferUsage, VmaMemoryUsage memoryUsage, VmaAllocationCreateFlags allocationFlags, VmaPool pool)
	: m_bufferHandle(VK_NULL_HANDLE)
	, m_allocation(VK_NULL_HANDLE)
	, m_bufferSize(0)
	, m_usageFlags(0)
{

}

RHIBuffer::~RHIBuffer()
{
	// TODO schedule remove
	SPT_CHECK_NO_ENTRY();
}

Uint64 RHIBuffer::GetSize() const
{
	return m_bufferSize;
}

Flags32 RHIBuffer::GetUsage() const
{
	return m_usageFlags;
}

void* RHIBuffer::GetMappedDataPtr() const
{
	// TODO 
	SPT_CHECK_NO_ENTRY();
	return nullptr;
}

DeviceAddress RHIBuffer::GetDeviceAddress() const
{
	VkBufferDeviceAddressInfo addressInfo{ VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO };
	addressInfo.buffer = m_bufferHandle;
	return vkGetBufferDeviceAddress(VulkanRHI::GetDeviceHandle(), &addressInfo);
}

void RHIBuffer::SetData(const void* data, Uint64 dataSize)
{

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

}
