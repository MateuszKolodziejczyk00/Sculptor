#pragma once

#include "RendererCoreMacros.h"
#include "SculptorCoreTypes.h"
#include "Types/Buffer.h"


namespace spt::rdr
{

struct ConstantBufferAllocation
{
	lib::SharedPtr<Buffer> buffer;
	Uint32                 offset = 0u;
	Uint32                 size   = 0u;
};


class RENDERER_CORE_API ConstantsAllocator
{
public:

	explicit ConstantsAllocator(Uint32 stackSize);

	void Reset() { m_currentOffset = 0u; }

	ConstantBufferAllocation Allocate(Uint32 size);

	const lib::SharedPtr<Buffer>& GetBuffer() const { return m_buffer; }

private:

	lib::SharedPtr<Buffer> m_buffer;

	std::atomic<Uint32> m_currentOffset    = 0u;
	Uint32              m_buffersAlignment = 0u;
};

} // spt::rdr
