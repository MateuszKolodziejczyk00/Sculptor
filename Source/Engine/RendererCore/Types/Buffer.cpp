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

BufferView Buffer::CreateFullView() const
{
	return BufferView(lib::Ref(const_cast<Buffer*>(this)->shared_from_this()), 0, GetRHI().GetSize());
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// BufferView ====================================================================================

BufferView::BufferView(const lib::SharedRef<Buffer>& buffer, Uint64 offset, Uint64 size)
	: m_buffer(buffer.ToSharedPtr())
	, m_offset(offset)
	, m_size(size)
{ }

void BufferView::Initialize(const lib::SharedRef<Buffer>& buffer, Uint64 offset, Uint64 size)
{
	m_buffer = buffer;
	m_offset = offset;
	m_size = size;
}

Bool BufferView::operator==(const BufferView& rhs) const
{
	return m_buffer == rhs.m_buffer
		&& m_offset == rhs.m_offset
		&& m_size == rhs.m_size;
}

const lib::SharedRef<Buffer>& BufferView::GetBuffer() const
{
	return m_buffer;
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
