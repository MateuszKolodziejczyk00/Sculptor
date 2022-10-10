#pragma once

#include "RHIMacros.h"
#include "SculptorCoreTypes.h"
#include "RHICore/RHIBufferTypes.h"
#include "Vulkan/VulkanCore.h"
#include "Vulkan/Debug/DebugUtils.h"


namespace spt::rhi
{
struct RHIAllocationInfo;
}


namespace spt::vulkan
{

class RHI_API RHIBuffer
{
public:

	RHIBuffer();

	void						InitializeRHI(Uint64 size, rhi::EBufferUsage bufferUsage, const rhi::RHIAllocationInfo& allocationInfo);
	void						ReleaseRHI();

	Bool						IsValid() const;

	Uint64						GetSize() const;
	rhi::EBufferUsage			GetUsage() const;

	void*						MapBufferMemory() const;
	void						UnmapBufferMemory() const;

	void*						GetMappedPtr() const;

	DeviceAddress				GetDeviceAddress() const;

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

	rhi::EBufferUsage			m_usageFlags;

	DebugName					m_name;

	EMappingStrategy			m_mappingStrategy;
	void*						m_mappedPointer;
};

}