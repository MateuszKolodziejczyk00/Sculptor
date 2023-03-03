#pragma once

#include "RendererCoreMacros.h"
#include "RHIBridge/RHIBufferImpl.h"
#include "RHICore/RHIAllocationTypes.h"
#include "RendererResource.h"


namespace spt::rhi
{
struct RHIAllocationInfo;
}


namespace spt::rdr
{

class BufferView;
struct RendererResourceName;


class RENDERER_CORE_API Buffer : public RendererResource<rhi::RHIBuffer>, public lib::SharedFromThis<Buffer>
{
protected:

	using ResourceType = RendererResource<rhi::RHIBuffer>;

public:

	Buffer(const RendererResourceName& name, const rhi::BufferDefinition& definition, const rhi::RHIAllocationInfo& allocationInfo);
	Buffer(const RendererResourceName& name, const rhi::RHIBuffer& rhiBufferInstance);

	BufferView CreateFullView() const;
};


class RENDERER_CORE_API BufferView
{
public:

	BufferView(const lib::SharedRef<Buffer>& buffer, Uint64 offset, Uint64 size);

	void Initialize(const lib::SharedRef<Buffer>& buffer, Uint64 offset, Uint64 size);

	Bool operator==(const BufferView& rhs) const;

	const lib::SharedRef<Buffer>& GetBuffer() const;

	Uint64 GetOffset() const;
	Uint64 GetSize() const;

private:

	lib::SharedRef<Buffer>	m_buffer;

	Uint64					m_offset;
	Uint64					m_size;
};

} // spt::rdr
