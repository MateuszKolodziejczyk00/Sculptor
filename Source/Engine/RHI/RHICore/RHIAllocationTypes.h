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
		: m_memoryUsage(EMemoryUsage::GPUOnly)
		, m_allocationFlags(EAllocationFlags::None)
	{ }

	EMemoryUsage				m_memoryUsage;
	EAllocationFlags			m_allocationFlags;
};

} // spt::rhi