#include "Barrier.h"

namespace spt::renderer
{

Barrier::Barrier()
{ }

rhi::RHIBarrier& Barrier::GetRHI()
{
	return m_rhiBarrier;
}

const rhi::RHIBarrier& Barrier::GetRHI() const
{
	return m_rhiBarrier;
}

}
