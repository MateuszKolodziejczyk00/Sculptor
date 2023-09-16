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

	for (const RayTracingHitGroupShaders& hitGroupShaders : shaders.hitGroups)
	{
		if (hitGroupShaders.closestHitShader)
		{
			AppendToPipelineMetaData(hitGroupShaders.closestHitShader->GetMetaData());
		}
		if (hitGroupShaders.anyHitShader)
		{
			AppendToPipelineMetaData(hitGroupShaders.anyHitShader->GetMetaData());
		}
		if (hitGroupShaders.intersectionShader)
		{
			AppendToPipelineMetaData(hitGroupShaders.intersectionShader->GetMetaData());
		}

		rhi::RayTracingHitGroupDefinition& rhiHitGroup = rayTracingShaders.hitGroups.emplace_back();
		rhiHitGroup.closestHitModule	= hitGroupShaders.closestHitShader		? hitGroupShaders.closestHitShader->GetRHI() : rhi::RHIShaderModule{};
		rhiHitGroup.anyHitModule		= hitGroupShaders.anyHitShader			? hitGroupShaders.anyHitShader->GetRHI() : rhi::RHIShaderModule{};
		rhiHitGroup.intersectionModule	= hitGroupShaders.intersectionShader	? hitGroupShaders.intersectionShader->GetRHI() : rhi::RHIShaderModule{};
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
