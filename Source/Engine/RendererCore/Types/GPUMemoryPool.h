#pragma once

#include "RendererCoreMacros.h"
#include "RHIBridge/RHIGPUMemoryPoolImpl.h"
#include "SculptorCoreTypes.h"
#include "RendererResource.h"
#include "RendererUtils.h"


namespace spt::rdr
{

class RENDERER_CORE_API GPUMemoryPool : public RendererResource<rhi::RHIGPUMemoryPool>
{
protected:

	using ResourceType = RendererResource<rhi::RHIGPUMemoryPool>;

public:

	GPUMemoryPool(const RendererResourceName& name, const rhi::RHIMemoryPoolDefinition& definition, const rhi::RHIAllocationInfo& allocationInfo);
};

} // spt::rdr
