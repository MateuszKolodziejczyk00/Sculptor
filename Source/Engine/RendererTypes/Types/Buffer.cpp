#include "Buffer.h"


namespace spt::renderer
{

Buffer::Buffer(Uint64 size, Flags32 bufferUsage, const rhi::RHIAllocationInfo& allocationInfo)
{
	m_rhiBuffer.InitializeRHI(size, bufferUsage, allocationInfo);
}

Buffer::~Buffer()
{

}

rhi::RHIBuffer& Buffer::GetRHI()
{
	return m_rhiBuffer;
}

const rhi::RHIBuffer& Buffer::GetRHI() const
{
	return m_rhiBuffer;
}

}
