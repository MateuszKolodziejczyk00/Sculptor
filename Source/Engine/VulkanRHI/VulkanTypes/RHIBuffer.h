#pragma once

#include "VulkanRHIMacros.h"
#include "Vulkan.h"
#include "Debug/DebugUtils.h"

#include "SculptorCoreTypes.h"
#include "RHIBufferTypes.h"


namespace spt::rhicore
{
struct RHIAllocationInfo;
}


namespace spt::vulkan
{

class VULKANRHI_API RHIBuffer
{
public:

	RHIBuffer();
	~RHIBuffer();

	// TODO Allocation info
	void						InitializeRHI(Uint64 size, Flags32 bufferUsage, const rhicore::RHIAllocationInfo& allocationInfo);
	void						ReleaseRHI();

	Bool						IsValid() const;

	Uint64						GetSize() const;
	Flags32						GetUsage() const;

	void*						MapBufferMemory() const;
	void						UnmapBufferMemory() const;

	DeviceAddress				GetDeviceAddress() const;

	Bool						CanSetData() const;

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