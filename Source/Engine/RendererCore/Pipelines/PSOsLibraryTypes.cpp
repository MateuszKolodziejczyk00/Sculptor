#include "PSOsLibraryTypes.h"
#include "PSOsLibrary.h"
#include "ResourcesManager.h"


namespace spt::rdr
{

ShaderID PSOImmediateCompiler::CompileShader(const lib::String& shaderRelativePath, const sc::ShaderStageCompilationDef& shaderStageDef, const sc::ShaderCompilationSettings& compilationSettings)
{
	return ResourcesManager::CreateShader(shaderRelativePath, shaderStageDef, compilationSettings);
}

PipelineStateID PSOImmediateCompiler::CreateComputePipeline(const RendererResourceName& name, const rdr::ShaderID& shader)
{
	return ResourcesManager::CreateComputePipeline(name, shader);
}

PipelineStateID PSOImmediateCompiler::CreateGraphicsPipeline(const RendererResourceName& name, const GraphicsPipelineShaders& shaders, const rhi::GraphicsPipelineDefinition& pipelineDef)
{
	return ResourcesManager::CreateGfxPipeline(name, shaders, pipelineDef);
}

void RegisterPSO(PSOPrecacheFunction callable)
{
	PSOsLibrary::GetInstance().RegisterPSO(callable);
}

} // spt::rdr