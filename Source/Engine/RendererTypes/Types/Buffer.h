#pragma once

#include "RendererTypesMacros.h"
#include "RHIBufferImpl.h"


namespace spt::rhi
{
struct RHIAllocationInfo;
}


namespace spt::renderer
{

class BufferView;
struct RendererResourceName;


class RENDERER_TYPES_API Buffer : public lib::SharedFromThis<Buffer>
{
public:

	Buffer(const RendererResourceName& name, Uint64 size, Flags32 bufferUsage, const rhi::RHIAllocationInfo& allocationInfo);
	~Buffer();

	rhi::RHIBuffer&					GetRHI();
	const rhi::RHIBuffer&			GetRHI() const;

	lib::SharedPtr<BufferView>		CreateView(Uint64 offset, Uint64 size) const;

private:

	rhi::RHIBuffer					m_rhiBuffer;
};


class RENDERER_TYPES_API BufferView
{
public:

	BufferView(const lib::SharedPtr<const Buffer>& buffer, Uint64 offset, Uint64 size);

	lib::SharedPtr<const Buffer>	GetBuffer() const;

	Uint64							GetOffset() const;
	Uint64							GetSize() const;

private:

	lib::WeakPtr<const Buffer>		m_buffer;

	Uint64							m_offset;
	Uint64							m_size;
};

}
