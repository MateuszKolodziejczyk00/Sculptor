#pragma once

#include "GraphicsMacros.h"
#include "SculptorCoreTypes.h"
#include "Types/Buffer.h"


namespace spt::gfx
{

class GRAPHICS_API UnifiedBuffer
{
public:

	explicit UnifiedBuffer(Uint64 size);

	rhi::RHISuballocation	Allocate(Uint64 size);
	void					Deallocate(rhi::RHISuballocation allocation);

	const lib::SharedPtr<rdr::Buffer>&		GetBufferInstance() const;
	const lib::SharedPtr<rdr::BufferView>&	GetBufferView() const;

private:

	lib::SharedPtr<rdr::Buffer>		m_bufferInstance;
	lib::SharedPtr<rdr::BufferView>	m_bufferView;
};

} // spt::gfx
