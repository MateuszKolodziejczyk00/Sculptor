#include "Buffer.h"
#include "RendererUtils.h"


namespace spt::rdr
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// Buffer ========================================================================================

Buffer::Buffer(const RendererResourceName& name, Uint64 size, rhi::EBufferUsage bufferUsage, const rhi::RHIAllocationInfo& allocationInfo)
{
	GetRHI().InitializeRHI(size, bufferUsage, allocationInfo);
	GetRHI().SetName(name.Get());
}

lib::SharedRef<BufferView> Buffer::CreateView(Uint64 offset, Uint64 size) const
{
	SPT_PROFILER_FUNCTION();

	return lib::MakeShared<BufferView>(lib::Ref(shared_from_this()), offset, size);
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// BufferView ====================================================================================

BufferView::BufferView(const lib::SharedRef<const Buffer>& buffer, Uint64 offset, Uint64 size)
	: m_buffer(buffer.ToSharedPtr())
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
