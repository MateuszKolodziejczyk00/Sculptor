#pragma once

#include "Pipeline.h"
#include "RendererUtils.h"
#include "RHICore/RHIPipelineDefinitionTypes.h"


namespace spt::rdr
{

class RENDERER_TYPES_API GraphicsPipeline : public Pipeline
{
public:

	GraphicsPipeline(const RendererResourceName& name, const lib::SharedRef<Shader>& shader, const rhi::GraphicsPipelineDefinition& pipelineDef);
};

} // spt::rdr
