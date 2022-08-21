#pragma once


namespace spt::rhi
{

enum class EMemoryUsage
{
	GPUOnly,
	CPUOnly,
	CPUToGPU,
	GPUToCpu,
	CPUCopy
};


enum class EAllocationFlags : Flags32
{
	None							= 0,
	CreateDedicatedAllocation		= BIT(0),
	CreateMapped					= BIT(1)
};


struct RHIAllocationInfo
{
	RHIAllocationInfo()
		: memoryUsage(EMemoryUsage::GPUOnly)
		, allocationFlags(EAllocationFlags::None)
	{ }

	EMemoryUsage				memoryUsage;
	EAllocationFlags			allocationFlags;
};

} // spt::rhi