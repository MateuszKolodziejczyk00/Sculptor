
#include "RendererTypesMacros.h"
#include "RHIBufferImpl.h"


namespace spt::rhi
{
struct RHIAllocationInfo;
}


namespace spt::renderer
{

class RENDERER_TYPES_API Buffer
{
public:

	Buffer(Uint64 size, Flags32 bufferUsage, const rhi::RHIAllocationInfo& allocationInfo);
	~Buffer();

	rhi::RHIBuffer& GetRHI();
	const rhi::RHIBuffer& GetRHI() const;

private:

	rhi::RHIBuffer			m_rhiBuffer;
};

}
