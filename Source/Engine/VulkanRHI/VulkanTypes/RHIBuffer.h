#pragma once

#include "SculptorCoreTypes.h"
#include "Vulkan.h"
#include "RHIBufferTypes.h"
#include "Debug/DebugUtils.h"


namespace spt::vulkan
{

class RHIBuffer
{
public:

	RHIBuffer(VmaAllocator allocator, Uint64 size, Flags32 bufferUsage, VmaMemoryUsage memoryUsage, VmaAllocationCreateFlags allocationFlags, VmaPool pool);
	~RHIBuffer();

	Uint64						GetSize() const;
	Flags32						GetUsage() const;
	void*						GetMappedDataPtr() const;

	DeviceAddress				GetDeviceAddress() const;

	void						SetData(const void* data, Uint64 dataSize);

	void						SetName(const lib::HashedString& name);
	const lib::HashedString&	GetName() const;

	// Vulkan =======================================================================

	VkBuffer					GetBufferHandle() const;
	VmaAllocation				GetAllocation() const;

private:

	VkBuffer					m_bufferHandle;

	VmaAllocation				m_allocation;

	Uint64						m_bufferSize;

	Flags32						m_usageFlags;

	DebugName					m_name;
};

}