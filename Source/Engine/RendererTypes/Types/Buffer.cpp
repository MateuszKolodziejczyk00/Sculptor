#include "Buffer.h"
#include "CurrentFrameContext.h"


namespace spt::renderer
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// Buffer ========================================================================================

Buffer::Buffer(Uint64 size, Flags32 bufferUsage, const rhi::RHIAllocationInfo& allocationInfo)
{
	m_rhiBuffer.InitializeRHI(size, bufferUsage, allocationInfo);
}

Buffer::~Buffer()
{
	CurrentFrameContext::SubmitDeferredRelease(
		[resource = m_rhiBuffer]() mutable
		{
			resource.ReleaseRHI();
		});
}

rhi::RHIBuffer& Buffer::GetRHI()
{
	return m_rhiBuffer;
}

const rhi::RHIBuffer& Buffer::GetRHI() const
{
	return m_rhiBuffer;
}

lib::SharedPtr<BufferView> Buffer::CreateView(Uint64 offset, Uint64 size) const
{
	return std::make_shared<BufferView>(shared_from_this(), offset, size);
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// BufferView ====================================================================================

BufferView::BufferView(const lib::SharedPtr<const Buffer>& buffer, Uint64 offset, Uint64 size)
	: m_buffer(buffer)
	, m_offset(offset)
	, m_size(size)
{ }

lib::SharedPtr<const Buffer> BufferView::GetBuffer() const
{
	return m_buffer.lock();
}

Uint64 BufferView::GetOffset() const
{
	return m_offset;
}

Uint64 BufferView::GetSize() const
{
	return m_size;
}

}
