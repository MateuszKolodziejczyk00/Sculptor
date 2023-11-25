#include "GPUMemoryPool.h"


namespace spt::rdr
{

GPUMemoryPool::GPUMemoryPool(const RendererResourceName& name, const rhi::RHIMemoryPoolDefinition& definition, const rhi::RHIAllocationInfo& allocationInfo)
{
	GetRHI().InitializeRHI(definition, allocationInfo);
	GetRHI().SetName(name.Get());
}

} // spt::rdr