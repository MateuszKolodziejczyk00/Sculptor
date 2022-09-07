#pragma once

#include "Pipeline.h"
#include "RendererResource.h"
#include "RHIBridge/RHIPipelineImpl.h"
#include "RendererUtils.h"


namespace spt::rdr
{

class RENDERER_TYPES_API ComputePipeline : public Pipeline, public RendererResource<rhi::RHIPipeline>
{
protected:

	using ResourceType = RendererResource<rhi::RHIPipeline>;

public:

	ComputePipeline(const RendererResourceName& name, const lib::SharedRef<Shader>& shader);
};

} // spt::rdr
