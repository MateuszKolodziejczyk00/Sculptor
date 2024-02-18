#pragma once

#include "RendererCoreMacros.h"
#include "RHIBridge/RHIDescriptorSetStackAllocatorImpl.h"
#include "SculptorCoreTypes.h"
#include "RendererResource.h"
#include "RendererUtils.h"


namespace spt::rdr
{

class RENDERER_CORE_API DescriptorSetStackAllocator : public RendererResource<rhi::RHIDescriptorSetStackAllocator>
{
protected:

	using ResourceType = RendererResource<rhi::RHIDescriptorSetStackAllocator>;

public:

	explicit DescriptorSetStackAllocator(const RendererResourceName& name);
};

} // spt::rdr
