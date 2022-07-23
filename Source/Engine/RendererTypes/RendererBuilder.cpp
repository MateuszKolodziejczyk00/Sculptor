#include "RendererBuilder.h"
#include "Types/Buffer.h"


namespace spt::renderer
{

lib::SharedPtr<Buffer> RendererBuilder::CreateBuffer(const RendererResourceName& name, Uint64 size, Flags32 bufferUsage, const rhi::RHIAllocationInfo& allocationInfo)
{
	return std::make_shared<Buffer>(name, size, bufferUsage, allocationInfo);
}

}
