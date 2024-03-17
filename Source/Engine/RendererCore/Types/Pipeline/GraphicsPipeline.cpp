#include "GraphicsPipeline.h"
#include "Types/Shader.h"

namespace spt::rdr
{

GraphicsPipeline::GraphicsPipeline(const RendererResourceName& name, const GraphicsPipelineShadersDefinition& shaders, const rhi::GraphicsPipelineDefinition& pipelineDef)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(!!shaders.vertexShader || !!shaders.meshShader);
	SPT_CHECK(!!shaders.fragmentShader && shaders.fragmentShader->GetStage() == rhi::EShaderStage::Fragment);

	rhi::GraphicsPipelineShadersDefinition shaderStagesDef;
	
	if (shaders.vertexShader)
	{
		SPT_CHECK(shaders.vertexShader->GetStage() == rhi::EShaderStage::Vertex);
		shaderStagesDef.vertexShader = shaders.vertexShader->GetRHI();
		AppendToPipelineMetaData(shaders.vertexShader->GetMetaData());
	}

	if (shaders.taskShader)
	{
		SPT_CHECK(shaders.taskShader->GetStage() == rhi::EShaderStage::Task);
		shaderStagesDef.taskShader = shaders.taskShader->GetRHI();
		AppendToPipelineMetaData(shaders.taskShader->GetMetaData());
	}

	if (shaders.meshShader)
	{
		SPT_CHECK(shaders.meshShader->GetStage() == rhi::EShaderStage::Mesh);
		shaderStagesDef.meshShader = shaders.meshShader->GetRHI();
		AppendToPipelineMetaData(shaders.meshShader->GetMetaData());
	}
	
	shaderStagesDef.fragmentShader = shaders.fragmentShader->GetRHI();
	AppendToPipelineMetaData(shaders.fragmentShader->GetMetaData());

	const rhi::PipelineLayoutDefinition pipelineLayoutDef = CreateLayoutDefinition();

	GetRHI().InitializeRHI(shaderStagesDef, pipelineDef, pipelineLayoutDef);
	GetRHI().SetName(name.Get());
}

} // spt::rdr

