#pragma once

#include "RendererCoreMacros.h"
#include "RHIBridge/RHISamplerImpl.h"
#include "SculptorCoreTypes.h"
#include "RendererResource.h"


namespace spt::rdr
{

class RENDERER_CORE_API Sampler : public RendererResource<rhi::RHISampler>
{
protected:

	using ResourceType = RendererResource<rhi::RHISampler>;

public:

	explicit Sampler(const rhi::SamplerDefinition& def);
};

} // spt::rdr
