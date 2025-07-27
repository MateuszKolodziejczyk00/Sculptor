#pragma once

#include "RendererCoreMacros.h"
#include "RHIBridge/RHIDescriptorHeapImpl.h"
#include "RendererResource.h"


namespace spt::rhi
{
struct DescriptorHeapDefinition;
}


namespace spt::rdr
{

struct RendererResourceName;


class RENDERER_CORE_API DescriptorHeap : public RendererResource<rhi::RHIDescriptorHeap>
{
protected:

	using ResourceType = RendererResource<rhi::RHIDescriptorHeap>;

public:

	DescriptorHeap(const RendererResourceName& name, const rhi::DescriptorHeapDefinition& definition);
};

} // spt::rdr
