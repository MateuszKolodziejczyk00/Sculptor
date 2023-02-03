#pragma once


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
	None							= 0,
	Unknown							= BIT(0),
	CreateDedicatedAllocation		= BIT(1),
	CreateMapped					= BIT(2)
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

	EMemoryUsage				memoryUsage;
	EAllocationFlags			allocationFlags;
};

} // spt::rhi