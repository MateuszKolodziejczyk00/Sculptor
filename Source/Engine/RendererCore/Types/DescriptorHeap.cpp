#include "DescriptorHeap.h"
#include "RendererUtils.h"


namespace spt::rdr
{

DescriptorHeap::DescriptorHeap(const RendererResourceName& name, const rhi::DescriptorHeapDefinition& definition)
{
	GetRHI().InitializeRHI(definition);
	GetRHI().SetName(name.Get());
}

} // spt::rdr
