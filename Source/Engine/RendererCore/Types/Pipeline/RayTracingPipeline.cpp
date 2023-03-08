#include "RayTracingPipeline.h"
#include "Types/Shader.h"
#include "CurrentFrameContext.h"

namespace spt::rdr
{

RayTracingPipeline::RayTracingPipeline(const RendererResourceName& name, const lib::SharedRef<Shader>& shader, const rhi::RayTracingPipelineDefinition& definition)
	: Super(shader)
{
	SPT_PROFILER_FUNCTION();

	const lib::DynamicArray<rhi::RHIShaderModule>& shaderModules = shader->GetShaderModules();

	rhi::RayTracingShadersDefinition rayTracingShaders;

	for (const rhi::RHIShaderModule& shaderModule : shaderModules)
	{
		switch (shaderModule.GetStage())
		{
		case rhi::EShaderStage::RTGeneration:
			SPT_CHECK(!rayTracingShaders.rayGenerationModule.IsValid());
			rayTracingShaders.rayGenerationModule = shaderModule;
			break;
		case rhi::EShaderStage::RTClosestHit:
			rayTracingShaders.closestHitModules.emplace_back(shaderModule);
			break;
		case rhi::EShaderStage::RTMiss:
			rayTracingShaders.missModules.emplace_back(shaderModule);
			break;
		default:
			SPT_CHECK_NO_ENTRY();
			break;
		}
	}

	const rhi::PipelineLayoutDefinition pipelineLayoutDef = CreateLayoutDefinition(*GetMetaData());

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
