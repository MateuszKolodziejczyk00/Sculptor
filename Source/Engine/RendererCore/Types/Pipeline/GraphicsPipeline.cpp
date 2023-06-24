#include "GraphicsPipeline.h"
#include "Types/Shader.h"

namespace spt::rdr
{

GraphicsPipeline::GraphicsPipeline(const RendererResourceName& name, const GraphicsPipelineShadersDefinition& shaders, const rhi::GraphicsPipelineDefinition& pipelineDef)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(!!shaders.vertexShader && shaders.vertexShader->GetStage() == rhi::EShaderStage::Vertex);
	SPT_CHECK(!!shaders.fragmentShader && shaders.fragmentShader->GetStage() == rhi::EShaderStage::Fragment);

	AppendToPipelineMetaData(shaders.vertexShader->GetMetaData());
	AppendToPipelineMetaData(shaders.fragmentShader->GetMetaData());

	rhi::GraphicsPipelineShadersDefinition shaderStagesDef;
	shaderStagesDef.vertexShader	= shaders.vertexShader->GetRHI();
	shaderStagesDef.fragmentShader	= shaders.fragmentShader->GetRHI();

	const rhi::PipelineLayoutDefinition pipelineLayoutDef = CreateLayoutDefinition();

	GetRHI().InitializeRHI(shaderStagesDef, pipelineDef, pipelineLayoutDef);
	GetRHI().SetName(name.Get());
}

} // spt::rdr

