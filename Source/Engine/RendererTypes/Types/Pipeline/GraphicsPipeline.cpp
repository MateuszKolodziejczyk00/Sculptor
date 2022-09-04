#include "GraphicsPipeline.h"
#include "Types/Shader.h"

namespace spt::rdr
{

GraphicsPipeline::GraphicsPipeline(const RendererResourceName& name, const lib::SharedPtr<Shader>& shader, const rhi::GraphicsPipelineDefinition& pipelineDef)
	: Pipeline(shader)
{
	SPT_PROFILE_FUNCTION();

	rhi::PipelineShaderStagesDefinition shaderStagesDef;
	shaderStagesDef.shaderModules = shader->GetShaderModules();

	const rhi::PipelineLayoutDefinition pipelineLayoutDef = CreateLayoutDefinition(*GetMetaData());

	GetRHI().InitializeRHI(shaderStagesDef, pipelineDef, pipelineLayoutDef);
	GetRHI().SetName(name.Get());
}

} // spt::rdr

