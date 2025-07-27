#pragma once

#include <variant>
#include "RHIBridge/RHIFwd.h"
#include "Utility/NamedType.h"


namespace spt::rhi
{

enum class EMemoryUsage : Flags8
{
	GPUOnly,
	CPUOnly,
	CPUToGPU,
	GPUToCpu,
	CPUCopy
};


enum class EAllocationFlags : Flags8
{
	None                      = 0,
	Unknown                   = BIT(0),
	CreateDedicatedAllocation = BIT(1),
	CreateMapped              = BIT(2)
};


struct RHIAllocationInfo
{
	RHIAllocationInfo()
		: memoryUsage(EMemoryUsage::GPUOnly)
		, allocationFlags(EAllocationFlags::None)
	{ }
	
	RHIAllocationInfo(EMemoryUsage inMemoryUsage, EAllocationFlags inAllocationFlags = EAllocationFlags::None)
		: memoryUsage(inMemoryUsage)
		, allocationFlags(inAllocationFlags)
	{ }

	Bool CanAccessOnCPU() const
	{
		return memoryUsage == EMemoryUsage::CPUOnly || memoryUsage == EMemoryUsage::CPUToGPU || memoryUsage == EMemoryUsage::CPUCopy;
	}

	EMemoryUsage     memoryUsage;
	EAllocationFlags allocationFlags;
};


enum class EVirtualAllocatorFlags
{
	None           = 0,
	Linear         = BIT(0),

	ClearOnRelease = BIT(1),

	Default        = None
};


enum class EVirtualAllocationFlags
{
	None                   = 0,
	PreferUpperAddress     = BIT(0),
	PreferMinMemory        = BIT(1),
	PreferFasterAllocation = BIT(2),
	PreferMinOffset        = BIT(3)
};


struct VirtualAllocationDefinition
{
	explicit VirtualAllocationDefinition(Uint64 inSize = 0, Uint64 inAlignment = 1, EVirtualAllocationFlags inFlags = EVirtualAllocationFlags::None)
		: size(inSize)
		, alignment(inAlignment)
		, flags(inFlags)
	{ }

	Uint64                  size;
	Uint64                  alignment;
	EVirtualAllocationFlags flags;
};


using RHIVirtualAllocationHandle = lib::NamedType<Uint64, struct RHIVirtualAllocationHandleTag>;


class RHIVirtualAllocation
{
public:

	RHIVirtualAllocation()
		: m_handle(RHIVirtualAllocationHandle{ 0 })
		, m_offset(0)
	{ }
	
	RHIVirtualAllocation(RHIVirtualAllocationHandle suballocationHandle, Uint64 offset)
		: m_handle(suballocationHandle)
		, m_offset(offset)
	{ }
	
	Bool IsValid() const
	{
		return m_handle != RHIVirtualAllocationHandle{ 0 };
	}

	Uint64 GetOffset() const
	{
		return m_offset;
	}

	RHIVirtualAllocationHandle GetHandle() const
	{
		return m_handle;
	}

	void Reset()
	{
		m_handle = RHIVirtualAllocationHandle{ 0 };
		m_offset = 0;
	}

private:

	RHIVirtualAllocationHandle m_handle;
	Uint64 m_offset;
};


struct RHIMemoryPoolDefinition
{
	explicit RHIMemoryPoolDefinition(Uint64 inSize, Uint64 inAlignment = 0u, EVirtualAllocatorFlags inAllocatorFlags = EVirtualAllocatorFlags::None)
		: size(inSize)
		, alignment(inAlignment)
		, allocatorFlags(inAllocatorFlags)
	{ }

	Uint64                 size;
	Uint64                 alignment;
	EVirtualAllocatorFlags allocatorFlags;
};


struct RHINullAllocation { };


class RHICommittedAllocation
{
public:

	RHICommittedAllocation()
		: m_handle(0)
	{ }

	explicit RHICommittedAllocation(Uint64 handle)
		: m_handle(handle)
	{ }

	Bool IsValid() const
	{
		return m_handle != 0;
	}

	Uint64 GetHandle() const
	{
		return m_handle;
	}

	void Reset()
	{
		m_handle = 0;
	}

private:

	Uint64 m_handle;
};


class RHIPlacedAllocation
{
public:

	RHIPlacedAllocation() = default;

	RHIPlacedAllocation(const RHICommittedAllocation& owningAllocation, const RHIVirtualAllocation& suballocation)
		: m_owningAllocation(owningAllocation)
		, m_suballocation(suballocation)
	{ }

	Bool IsValid() const
	{
		return m_owningAllocation.IsValid() && m_suballocation.IsValid();
	}

	const RHICommittedAllocation& GetOwningAllocation() const
	{
		return m_owningAllocation;
	}

	const RHIVirtualAllocation& GetSuballocation() const
	{
		return m_suballocation;
	}

	void Reset()
	{
		m_owningAllocation.Reset();
		m_suballocation.Reset();
	}

private:

	RHICommittedAllocation m_owningAllocation;
	RHIVirtualAllocation   m_suballocation;
};


class RHIExternalAllocation
{
public:

	RHIExternalAllocation() = default;
};


using RHIResourceAllocationHandle = std::variant<RHINullAllocation, RHICommittedAllocation, RHIPlacedAllocation, RHIExternalAllocation>;


struct RHINullAllocationDefinition { };


struct RHICommittedAllocationDefinition
{
	RHICommittedAllocationDefinition() = default;

	explicit RHICommittedAllocationDefinition(RHIAllocationInfo inAllocationInfo, Uint64 inAlignment)
		: allocationInfo(inAllocationInfo)
		, alignment(inAlignment)
	{ }

	RHICommittedAllocationDefinition(EMemoryUsage inMemoryUsage, EAllocationFlags inAllocationFlags = EAllocationFlags::None)
		: allocationInfo(inMemoryUsage, inAllocationFlags)
	{ }

	RHIAllocationInfo allocationInfo;
	Uint64            alignment = 0u;
};


struct RHIPlacedAllocationDefinition
{
	RHIGPUMemoryPool*       pool  = nullptr;
	EVirtualAllocationFlags flags = EVirtualAllocationFlags::None;
};


using RHIResourceAllocationDefinition = std::variant<RHINullAllocationDefinition, RHICommittedAllocationDefinition, RHIPlacedAllocationDefinition>;


struct RHIMemoryRequirements
{
	Uint64 size      = 0;
	Uint64 alignment = 0;
};

} // spt::rhi