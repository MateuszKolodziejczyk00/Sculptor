#include "RayTracingPipeline.h"
#include "Types/Shader.h"
#include "CurrentFrameContext.h"

namespace spt::rdr
{

RayTracingPipeline::RayTracingPipeline(const RendererResourceName& name, const lib::DynamicArray<lib::SharedRef<Shader>>& shaders, const rhi::RayTracingPipelineDefinition& definition)
{
	SPT_PROFILER_FUNCTION();

	rhi::RayTracingShadersDefinition rayTracingShaders;

	for (const lib::SharedRef<Shader>& shader : shaders)
	{
		AppendToPipelineMetaData(shader->GetMetaData());

		const rhi::RHIShaderModule& shaderModule = shader->GetRHI();

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
