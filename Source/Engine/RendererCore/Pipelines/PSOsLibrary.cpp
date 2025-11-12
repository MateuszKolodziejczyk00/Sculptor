#include "PSOsLibrary.h"
#include "ResourcesManager.h"
#include "JobSystem.h"


namespace spt::rdr
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// PSOsPrecachingCompiler ========================================================================

class PSOsPrecachingCompiler : public PSOCompilerInterface
{
public:

	PSOsPrecachingCompiler() = default;

	virtual ShaderID CompileShader(const lib::String& shaderRelativePath, const sc::ShaderStageCompilationDef& shaderStageDef, const sc::ShaderCompilationSettings& compilationSettings) override;

	virtual PipelineStateID CreateComputePipeline(const RendererResourceName& name, const ShaderID& shader) override;
	virtual PipelineStateID CreateGraphicsPipeline(const RendererResourceName& name, const GraphicsPipelineShaders& shaders, const rhi::GraphicsPipelineDefinition& pipelineDef) override;
	virtual PipelineStateID CreateRayTracingPipeline(const RendererResourceName& name, const RayTracingPipelineShaders& shaders, const rhi::RayTracingPipelineDefinition& pipelineDef) override;

	void ExecutePrecaching();

private:

	lib::DynamicArray<std::tuple<lib::String, sc::ShaderStageCompilationDef, sc::ShaderCompilationSettings>> m_shaderCompilationRequests;

	lib::DynamicArray<std::tuple<RendererResourceName, ShaderID>> m_computePSOsRequests;

	lib::DynamicArray<std::tuple<RendererResourceName, GraphicsPipelineShaders, rhi::GraphicsPipelineDefinition>> m_graphicsPSOsRequests;

	lib::DynamicArray<std::tuple<RendererResourceName, RayTracingPipelineShaders, rhi::RayTracingPipelineDefinition>> m_rayTracingPSOsRequests;
};

ShaderID PSOsPrecachingCompiler::CompileShader(const lib::String& shaderRelativePath, const sc::ShaderStageCompilationDef& shaderStageDef, const sc::ShaderCompilationSettings& compilationSettings)
{
	m_shaderCompilationRequests.emplace_back(shaderRelativePath, shaderStageDef, compilationSettings);
	return ResourcesManager::GenerateShaderID(shaderRelativePath, shaderStageDef, compilationSettings);
}

PipelineStateID PSOsPrecachingCompiler::CreateComputePipeline(const RendererResourceName& name, const rdr::ShaderID& shader)
{
	m_computePSOsRequests.emplace_back(name, shader);
	return ResourcesManager::GeneratePipelineID(shader);
}

PipelineStateID PSOsPrecachingCompiler::CreateGraphicsPipeline(const RendererResourceName& name, const GraphicsPipelineShaders& shaders, const rhi::GraphicsPipelineDefinition& pipelineDef)
{
	m_graphicsPSOsRequests.emplace_back(name, shaders, pipelineDef);
	return ResourcesManager::GeneratePipelineID(shaders, pipelineDef);
}

PipelineStateID PSOsPrecachingCompiler::CreateRayTracingPipeline(const RendererResourceName& name, const RayTracingPipelineShaders& shaders, const rhi::RayTracingPipelineDefinition& pipelineDef)
{
	m_rayTracingPSOsRequests.emplace_back(name, shaders, pipelineDef);
	return ResourcesManager::GeneratePipelineID(shaders, pipelineDef);
}

void PSOsPrecachingCompiler::ExecutePrecaching()
{
	SPT_PROFILER_FUNCTION();

	js::InlineParallelForEach(SPT_GENERIC_JOB_NAME, m_shaderCompilationRequests,
							  [](const std::tuple<lib::String, sc::ShaderStageCompilationDef, sc::ShaderCompilationSettings>& request)
							  {
								  SPT_PROFILER_SCOPE(std::get<0>(request).c_str());
								  ResourcesManager::CreateShader(std::get<0>(request), std::get<1>(request), std::get<2>(request));
							  });

//	js::LaunchInline(SPT_GENERIC_JOB_NAME,
//					[this]
//					{
//
//						js::InlineParallelForEach(SPT_GENERIC_JOB_NAME, m_computePSOsRequests,
//												  [](const std::tuple<RendererResourceName, ShaderID>& request)
//												  {
//#if RENDERER_VALIDATION
//													  SPT_PROFILER_SCOPE(std::get<0>(request).Get().ToString().c_str());
//#else
//													  SPT_PROFILER_SCOPE("Create Compute PSO");
//#endif // RENDERER_VALIDATION
//													  ResourcesManager::CreateComputePipeline(std::get<0>(request), std::get<1>(request));
//												  });
//		
//						js::InlineParallelForEach(SPT_GENERIC_JOB_NAME, m_graphicsPSOsRequests,
//												  [](const std::tuple<RendererResourceName, GraphicsPipelineShaders, rhi::GraphicsPipelineDefinition>& request)
//												  {
//#if RENDERER_VALIDATION
//													  SPT_PROFILER_SCOPE(std::get<0>(request).Get().ToString().c_str());
//#else
//													  SPT_PROFILER_SCOPE("Create Graphics PSO");
//#endif // RENDERER_VALIDATION
//													  ResourcesManager::CreateGfxPipeline(std::get<0>(request), std::get<1>(request), std::get<2>(request));
//												  });
//
//						js::InlineParallelForEach(SPT_GENERIC_JOB_NAME, m_rayTracingPSOsRequests,
//												  [](const std::tuple<RendererResourceName, RayTracingPipelineShaders, rhi::RayTracingPipelineDefinition>& request)
//												  {
//#if RENDERER_VALIDATION
//													  SPT_PROFILER_SCOPE(std::get<0>(request).Get().ToString().c_str());
//#else
//													  SPT_PROFILER_SCOPE("Create Ray Tracing PSO");
//#endif // RENDERER_VALIDATION
//													  ResourcesManager::CreateRayTracingPipeline(std::get<0>(request), std::get<1>(request), std::get<2>(request));
//												  });
//					});

	js::LaunchInline("Precaching Scope",
					 [=, this]()
	{
		for (const auto& request : m_rayTracingPSOsRequests)
		{
			js::AddNested("Ray Tracing PSO Creation",
						   [=]()
			{
#if RENDERER_VALIDATION
				SPT_PROFILER_SCOPE(std::get<0>(request).Get().ToString().c_str());
#endif // RENDERER_VALIDATION
				ResourcesManager::CreateRayTracingPipeline(std::get<0>(request), std::get<1>(request), std::get<2>(request));
			});
		}

		for (const auto& request : m_graphicsPSOsRequests)
		{
			js::AddNested("Graphics PSO Creation",
						   [=]()
			{
#if RENDERER_VALIDATION
				SPT_PROFILER_SCOPE(std::get<0>(request).Get().ToString().c_str());
#endif // RENDERER_VALIDATION
				ResourcesManager::CreateGfxPipeline(std::get<0>(request), std::get<1>(request), std::get<2>(request));
			});
		}

		for (const auto& request : m_computePSOsRequests)
		{
			js::AddNested("Compute PSO Creation",
													 [=]()
			{
#if RENDERER_VALIDATION
				SPT_PROFILER_SCOPE(std::get<0>(request).Get().ToString().c_str());
#endif // RENDERER_VALIDATION
				ResourcesManager::CreateComputePipeline(std::get<0>(request), std::get<1>(request));
			});
		}
	});
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// PSOsLibrary ===================================================================================

PSOsLibrary& PSOsLibrary::GetInstance()
{
	static PSOsLibrary instance;
	return instance;
}

void PSOsLibrary::RegisterPSO(PSOPrecacheFunction pso)
{
	m_registeredPSOs.emplace_back(pso);
}

void PSOsLibrary::PrecachePSOs(const PSOPrecacheParams& precacheParams)
{
	PSOsPrecachingCompiler compiler;

	for (PSOPrecacheFunction precache : m_registeredPSOs)
	{
		precache(compiler, precacheParams);
	}

	compiler.ExecutePrecaching();
}

} // spt::rdr