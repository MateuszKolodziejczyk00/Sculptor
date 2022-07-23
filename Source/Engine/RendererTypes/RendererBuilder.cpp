#include "RendererBuilder.h"
#include "Types/Buffer.h"


namespace spt::renderer
{

lib::SharedPtr<Buffer> RendererBuilder::CreateBuffer(Uint64 size, Flags32 bufferUsage, const rhi::RHIAllocationInfo& allocationInfo)
{
	return std::make_shared<Buffer>(size, bufferUsage, allocationInfo);
}

}
