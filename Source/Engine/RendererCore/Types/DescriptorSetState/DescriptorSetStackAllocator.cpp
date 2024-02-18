#include "DescriptorSetStackAllocator.h"


namespace spt::rdr
{

DescriptorSetStackAllocator::DescriptorSetStackAllocator(const RendererResourceName& name)
{
	GetRHI().InitializeRHI();
	GetRHI().SetName(name.Get());
}

} // spt::rdr
