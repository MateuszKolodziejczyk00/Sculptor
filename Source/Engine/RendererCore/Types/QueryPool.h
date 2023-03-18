#pragma once

#include "RendererCoreMacros.h"
#include "RHIBridge/RHIQueryPoolImpl.h"
#include "SculptorCoreTypes.h"
#include "RendererResource.h"


namespace spt::rdr
{

class RENDERER_CORE_API QueryPool : public RendererResource<rhi::RHIQueryPool>
{
protected:

	using ResourceType = RendererResource<rhi::RHIQueryPool>;

public:

	explicit QueryPool(const rhi::QueryPoolDefinition& definition);

	void Reset();
};

} // spt::rdr