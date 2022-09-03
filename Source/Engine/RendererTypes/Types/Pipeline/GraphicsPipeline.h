#pragma once

#include "Pipeline.h"
#include "RendererResource.h"
#include "RHIBridge/RHIPipelineImpl.h"
#include "RendererUtils.h"
#include "RHICore/RHIPipelineDefinitionTypes.h"


namespace spt::rdr
{

class RENDERER_TYPES_API GraphicsPipeline : public Pipeline, public RendererResource<rhi::RHIPipeline>
{
protected:

	using ResourceType = RendererResource<rhi::RHIPipeline>;

public:

	GraphicsPipeline(const RendererResourceName& name, const lib::SharedPtr<Shader>& shader, const rhi::GraphicsPipelineDefinition pipelineDef);
};

} // spt::renderer
