#pragma once

#include "RHIMacros.h"
#include "SculptorCoreTypes.h"
#include "RHICore/RHIBufferTypes.h"
#include "Vulkan/VulkanCore.h"
#include "Vulkan/Debug/DebugUtils.h"
#include "Vulkan/Memory/VulkanMemoryTypes.h"


namespace spt::rhi
{
struct RHIAllocationInfo;
}


namespace spt::vulkan
{

class RHIBuffer;
class RHIGPUMemoryPool;


class RHI_API RHIMappedByteBuffer
{
public:

	explicit RHIMappedByteBuffer(const RHIBuffer& buffer);

	~RHIMappedByteBuffer();

	RHIMappedByteBuffer(const RHIMappedByteBuffer&) = delete;
	RHIMappedByteBuffer& operator=(const RHIMappedByteBuffer&) = delete;

	RHIMappedByteBuffer(RHIMappedByteBuffer&& other);
	RHIMappedByteBuffer& operator=(RHIMappedByteBuffer&& other) = delete;

	Byte* GetPtr() const;

	Uint64 GetSize() const;

	lib::Span<Byte> GetSpan() const { return lib::Span<Byte>(GetPtr(), GetSize()); }

private:

	const RHIBuffer&	m_buffer;
	Byte*				m_mappedPointer;
};


template<typename TDataType>
class RHIMappedBuffer : public RHIMappedByteBuffer
{
protected:

	using Super = RHIMappedByteBuffer;

public:

	explicit RHIMappedBuffer(const RHIBuffer& buffer)
		: Super(buffer)
	{ }

	TDataType* Get() const
	{
		return reinterpret_cast<TDataType*>(Super::GetPtr());
	}

	TDataType& operator[](Uint64 idx) const
	{
		return Get()[idx];
	}

	Uint64 GetElementsNum() const
	{
		return Super::GetSize() / sizeof(TDataType);
	}
};


struct RHI_API RHIBufferReleaseTicket
{
	void ExecuteReleaseRHI();

	RHIResourceReleaseTicket<VkBuffer> handle;
	RHIResourceReleaseTicket<VmaAllocation> allocation;

#if SPT_RHI_DEBUG
	lib::HashedString name;
#endif // SPT_RHI_DEBUG
};


class RHI_API RHIBuffer
{
public:

	RHIBuffer();

	void						InitializeRHI(const rhi::BufferDefinition& definition, const rhi::RHIResourceAllocationDefinition& allocationDef);
	void						ReleaseRHI();

	RHIBufferReleaseTicket		DeferredReleaseRHI();

	Bool						IsValid() const;

	void						CopySRVDescriptor(Uint64 offset, Uint64 range, Byte* dst) const;
	void						CopyUAVDescriptor(Uint64 offset, Uint64 range, Byte* dst) const;
	void						CopyTLASDescriptor(Byte* dst) const;

	Uint64						GetSize() const;
	rhi::EBufferUsage			GetUsage() const;

	Bool						HasBoundMemory() const;

	Bool						CanMapMemory() const;
	Byte*						MapPtr() const;
	void						Unmap() const;

	DeviceAddress				GetDeviceAddress() const;

	Bool						AllowsSuballocations() const;
	rhi::RHIVirtualAllocation	CreateSuballocation(const rhi::VirtualAllocationDefinition& definition);
	void						DestroySuballocation(rhi::RHIVirtualAllocation suballocation);
	void						DestroySuballocation(rhi::RHIVirtualAllocationHandle suballocation);

	rhi::RHIMemoryRequirements	GetMemoryRequirements() const;

	void						SetName(const lib::HashedString& name);
	const lib::HashedString&	GetName() const;

	// Vulkan =======================================================================

	VkBuffer					GetHandle() const;
	VkBufferUsageFlags			GetVulkanUsage() const;

private:

	enum class EMappingStrategy
	{
		PersistentlyMapped,
		MappedWhenNecessary,
		CannotBeMapped
	};

	Bool                             BindMemory(const rhi::RHIResourceAllocationDefinition& allocationDefinition);
	rhi::RHIResourceAllocationHandle ReleasePlacedAllocation();

	rhi::RHIResourceAllocationHandle DoPlacedAllocation(const rhi::RHIPlacedAllocationDefinition& placedAllocationDef);
	rhi::RHIResourceAllocationHandle DoCommittedAllocation(const rhi::RHICommittedAllocationDefinition& committedAllocation);

	void          PreUnbindMemory(VmaAllocation allocation);
	VmaAllocation GetAllocation() const;

	void             InitializeMappingStrategy(const rhi::RHIAllocationInfo& allocationInfo);
	EMappingStrategy SelectMappingStrategy(const rhi::RHIAllocationInfo& allocationInfo) const;

	VkBuffer                         m_bufferHandle;
	rhi::RHIResourceAllocationHandle m_allocationHandle;

	Uint64            m_bufferSize;
	rhi::EBufferUsage m_usageFlags;

	EMappingStrategy m_mappingStrategy;
	Byte*            m_mappedPointer;

	VulkanVirtualAllocator m_virtualAllocator;

	DebugName m_name;

	friend class RHIBufferMemoryOwner;
};


class RHI_API RHIBufferMemoryOwner
{
protected:

	static Bool                             BindMemory(RHIBuffer& buffer, const rhi::RHIResourceAllocationDefinition& allocationDefinition);
	static rhi::RHIResourceAllocationHandle ReleasePlacedAllocation(RHIBuffer& buffer);
};

} // spt::vulkan