#pragma once

#include "RendererCoreMacros.h"
#include "RendererResource.h"
#include "RHIBridge/RHIEventImpl.h"
#include "RendererUtils.h"


namespace spt::rdr
{

class RENDERER_CORE_API GPUEvent : public RendererResource<rhi::RHIEvent>
{
protected:

	using ResourceType = RendererResource<rhi::RHIEvent>;

public:

	GPUEvent(const RendererResourceName& name, const rhi::EventDefinition& definition);
};

} // spt::rdr