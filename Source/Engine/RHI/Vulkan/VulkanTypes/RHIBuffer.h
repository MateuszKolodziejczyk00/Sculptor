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

	void						InitializeRHI(const rhi::BufferDefinition& definition, const rhi::RHIAllocationInfo& allocationInfo);
	void						ReleaseRHI();

	Bool						IsValid() const;

	Uint64						GetSize() const;
	rhi::EBufferUsage			GetUsage() const;

	Bool						CanMapMemory() const;
	Byte*						MapBufferMemory() const;
	void						UnmapBufferMemory() const;

	DeviceAddress				GetDeviceAddress() const;

	Bool						AllowsSuballocations() const;
	rhi::RHISuballocation		CreateSuballocation(const rhi::SuballocationDefinition& definition);
	void						DestroySuballocation(rhi::RHISuballocation suballocation);

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

	void						InitVirtualBlock(const rhi::BufferDefinition& definition);

	VkBuffer					m_bufferHandle;

	VmaAllocation				m_allocation;

	Uint64						m_bufferSize;

	rhi::EBufferUsage			m_usageFlags;

	DebugName					m_name;

	EMappingStrategy			m_mappingStrategy;
	Byte*						m_mappedPointer;

	VmaVirtualBlock				m_allocatorVirtualBlock;
};

} // spt::vulkan