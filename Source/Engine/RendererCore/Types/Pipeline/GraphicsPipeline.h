#pragma once

#include "Pipeline.h"
#include "RendererUtils.h"
#include "RHICore/RHIPipelineDefinitionTypes.h"


namespace spt::rdr
{

struct GraphicsPipelineShadersDefinition
{
	lib::SharedPtr<Shader> vertexShader;
	lib::SharedPtr<Shader> taskShader;
	lib::SharedPtr<Shader> meshShader;
	lib::SharedPtr<Shader> fragmentShader;
};


class RENDERER_CORE_API GraphicsPipeline : public Pipeline
{
public:

	GraphicsPipeline(const RendererResourceName& name, const GraphicsPipelineShadersDefinition& shaders, const rhi::GraphicsPipelineDefinition& pipelineDef);
};

} // spt::rdr
