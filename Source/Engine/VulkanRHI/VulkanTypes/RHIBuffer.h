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

	RHIBuffer(Uint64 size, Flags32 bufferUsage, const VmaAllocationCreateInfo& allocationInfo);
	~RHIBuffer();

	Uint64						GetSize() const;
	Flags32						GetUsage() const;

	void*						MapBufferMemory() const;
	void						UnmapBufferMemory() const;

	DeviceAddress				GetDeviceAddress() const;

	bool						CanSetData() const;

	void						SetData(const void* data, Uint64 dataSize);

	void						SetName(const lib::HashedString& name);
	const lib::HashedString&	GetName() const;

	// Vulkan =======================================================================

	VkBuffer					GetBufferHandle() const;
	VmaAllocation				GetAllocation() const;

private:

	enum class EMappingStrategy
	{
		PersistentlyMapped,
		MappedWhenNecessary,
		CannotBeMapped
	};

	void						InitializeRHI(Uint64 size, Flags32 bufferUsage, const VmaAllocationCreateInfo& allocationInfo);

	void						InitializeMappingStrategy(const VmaAllocationCreateInfo& allocationInfo);
	EMappingStrategy			SelectMappingStrategy(const VmaAllocationCreateInfo& allocationInfo) const;

	VkBuffer					m_bufferHandle;

	VmaAllocation				m_allocation;

	Uint64						m_bufferSize;

	Flags32						m_usageFlags;

	DebugName					m_name;

	EMappingStrategy			m_mappingStrategy;
	void*						m_mappedPointer;
};

}