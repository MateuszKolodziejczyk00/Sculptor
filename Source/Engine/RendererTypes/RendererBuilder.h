#pragma once

#include "RendererTypesMacros.h"
#include "SculptorCoreTypes.h"
#include "RendererUtils.h"
#include "RHIFwd.h"


namespace spt::rhi
{
struct RHIAllocationInfo;
}


namespace spt::renderer
{

class Buffer;


class RENDERER_TYPES_API RendererBuilder
{
public:

	static lib::SharedPtr<Buffer>				CreateBuffer(const RendererResourceName& name, Uint64 size, Flags32 bufferUsage, const rhi::RHIAllocationInfo& allocationInfo);
};

}