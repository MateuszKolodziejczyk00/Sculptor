#pragma once

#include "RendererCoreMacros.h"
#include "RHIBridge/RHISemaphoreImpl.h"
#include "RendererResource.h"


namespace spt::rhi
{
struct SemaphoreDefinition;
}


namespace spt::rdr
{

struct RendererResourceName;


class RENDERER_CORE_API Semaphore : public RendererResource<rhi::RHISemaphore>
{
protected:

	using ResourceType = RendererResource<rhi::RHISemaphore>;

public:

	Semaphore(const RendererResourceName& name, const rhi::SemaphoreDefinition& definition);
};

}