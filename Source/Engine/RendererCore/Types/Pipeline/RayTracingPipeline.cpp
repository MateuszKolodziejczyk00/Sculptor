#include "RayTracingPipeline.h"
#include "Types/Shader.h"
#include "CurrentFrameContext.h"

namespace spt::rdr
{

RayTracingPipeline::RayTracingPipeline(const RendererResourceName& name, const RayTracingPipelineShaderObjects& shaders, const rhi::RayTracingPipelineDefinition& definition)
{
	SPT_PROFILER_FUNCTION();

	rhi::RayTracingShadersDefinition rayTracingShaders;

	AppendToPipelineMetaData(shaders.rayGenShader->GetMetaData());
	rayTracingShaders.rayGenerationModule = shaders.rayGenShader->GetRHI();

	for (const lib::SharedRef<Shader>& shader : shaders.closestHitShaders)
	{
		AppendToPipelineMetaData(shader->GetMetaData());
		rayTracingShaders.closestHitModules.emplace_back(shader->GetRHI());
	}

	for (const lib::SharedRef<Shader>& shader : shaders.missShaders)
	{
		AppendToPipelineMetaData(shader->GetMetaData());
		rayTracingShaders.missModules.emplace_back(shader->GetRHI());
	}
	
	const rhi::PipelineLayoutDefinition pipelineLayoutDef = CreateLayoutDefinition();

	rhi::RHIPipeline& rhiPipeline = GetRHI();

	rhiPipeline.InitializeRHI(rayTracingShaders, definition, pipelineLayoutDef);
	rhiPipeline.SetName(name.Get());

	m_shaderBindingTable.InitializeRHI(rhiPipeline, rayTracingShaders);
}

RayTracingPipeline::~RayTracingPipeline()
{
	SPT_PROFILER_FUNCTION();

	CurrentFrameContext::GetCurrentFrameCleanupDelegate().AddLambda([ resource = m_shaderBindingTable ]() mutable
																	{
																		resource.ReleaseRHI();
																	});
}

const spt::rhi::RHIShaderBindingTable& RayTracingPipeline::GetShaderBindingTable() const
{
	return m_shaderBindingTable;
}

} // spt::rdr
