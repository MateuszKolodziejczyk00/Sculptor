#pragma once

#include "RendererTypesMacros.h"
#include "SculptorCoreTypes.h"
#include "RHIBridge/RHIContextImpl.h"
#include "RendererResource.h"


namespace spt::rdr
{

struct RendererResourceName;


class RENDERER_TYPES_API Context : public RendererResource<rhi::RHIContext>
{
protected:

	using ResourceType = RendererResource<rhi::RHIContext>;

public:

	Context(const RendererResourceName& name, const rhi::ContextDefinition& contextDef);
};

} // spt::rdr