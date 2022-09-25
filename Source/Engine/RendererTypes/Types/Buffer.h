#pragma once

#include "RendererTypesMacros.h"
#include "RHIBridge/RHIBufferImpl.h"
#include "RendererResource.h"


namespace spt::rhi
{
struct RHIAllocationInfo;
}


namespace spt::rdr
{

class BufferView;
struct RendererResourceName;


class RENDERER_TYPES_API Buffer : public RendererResource<rhi::RHIBuffer>, public lib::SharedFromThis<Buffer>
{
protected:

	using ResourceType = RendererResource<rhi::RHIBuffer>;

public:

	Buffer(const RendererResourceName& name, Uint64 size, rhi::EBufferUsage bufferUsage, const rhi::RHIAllocationInfo& allocationInfo);

	lib::SharedRef<BufferView>		CreateView(Uint64 offset, Uint64 size) const;
};


class RENDERER_TYPES_API BufferView
{
public:

	BufferView(const lib::SharedRef<const Buffer>& buffer, Uint64 offset, Uint64 size);

	lib::SharedPtr<const Buffer>	GetBuffer() const;

	Uint64							GetOffset() const;
	Uint64							GetSize() const;

private:

	lib::WeakPtr<const Buffer>		m_buffer;

	Uint64							m_offset;
	Uint64							m_size;
};

} // spt::rdr
