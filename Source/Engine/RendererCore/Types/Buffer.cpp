#include "Buffer.h"
#include "RendererUtils.h"


namespace spt::rdr
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// Buffer ========================================================================================

Buffer::Buffer(const RendererResourceName& name, const rhi::BufferDefinition& definition, const rhi::RHIAllocationInfo& allocationInfo)
{
	GetRHI().InitializeRHI(definition, allocationInfo);
	GetRHI().SetName(name.Get());
}

lib::SharedRef<BufferView> Buffer::CreateView(Uint64 offset, Uint64 size) const
{
	SPT_PROFILER_FUNCTION();

	return lib::MakeShared<BufferView>(lib::Ref(const_cast<Buffer*>(this)->shared_from_this()), offset, size);
}

lib::SharedRef<rdr::BufferView> Buffer::CreateFullView() const
{
	return CreateView(0, GetRHI().GetSize());
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// BufferView ====================================================================================

BufferView::BufferView(const lib::SharedRef<Buffer>& buffer, Uint64 offset, Uint64 size)
	: m_buffer(buffer.ToSharedPtr())
	, m_offset(offset)
	, m_size(size)
{ }

lib::SharedPtr<Buffer> BufferView::GetBuffer() const
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
