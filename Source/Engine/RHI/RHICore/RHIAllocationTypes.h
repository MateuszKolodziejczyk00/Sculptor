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

namespace EAllocationFlags
{

enum Flags : Flags32
{
	CreateDedicatedAllocation		= BIT(0),
	CreateMapped					= BIT(1)
};

}

struct RHIAllocationInfo
{
	RHIAllocationInfo()
		: m_memoryUsage(EMemoryUsage::GPUOnly)
		, m_allocationFlags(0)
	{ }

	EMemoryUsage				m_memoryUsage;
	Flags32						m_allocationFlags;
};

}