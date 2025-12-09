#include "PSOsLibrary.h"
#include "ResourcesManager.h"
#include "JobSystem.h"


SPT_DEFINE_LOG_CATEGORY(PSOsLibrary, true);

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

	lib::HashSet<ShaderID>        m_registeredShaders;
	lib::HashSet<PipelineStateID> m_registeredPipelines;
};

ShaderID PSOsPrecachingCompiler::CompileShader(const lib::String& shaderRelativePath, const sc::ShaderStageCompilationDef& shaderStageDef, const sc::ShaderCompilationSettings& compilationSettings)
{
	const ShaderID shaderID = ResourcesManager::GenerateShaderID(shaderRelativePath, shaderStageDef, compilationSettings);
	if (!m_registeredShaders.contains(shaderID))
	{
		m_shaderCompilationRequests.emplace_back(shaderRelativePath, shaderStageDef, compilationSettings);
		m_registeredShaders.insert(shaderID);
	}
	return shaderID;
}

PipelineStateID PSOsPrecachingCompiler::CreateComputePipeline(const RendererResourceName& name, const rdr::ShaderID& shader)
{
	const PipelineStateID pipelineID = ResourcesManager::GeneratePipelineID(shader);
	if (!m_registeredPipelines.contains(pipelineID))
	{
		m_computePSOsRequests.emplace_back(name, shader);
		m_registeredPipelines.insert(pipelineID);
	}
	return pipelineID;
}

PipelineStateID PSOsPrecachingCompiler::CreateGraphicsPipeline(const RendererResourceName& name, const GraphicsPipelineShaders& shaders, const rhi::GraphicsPipelineDefinition& pipelineDef)
{
	const PipelineStateID pipelineID = ResourcesManager::GeneratePipelineID(shaders, pipelineDef);
	if (!m_registeredPipelines.contains(pipelineID))
	{
		m_graphicsPSOsRequests.emplace_back(name, shaders, pipelineDef);
		m_registeredPipelines.insert(pipelineID);
	}
	return pipelineID;
}

PipelineStateID PSOsPrecachingCompiler::CreateRayTracingPipeline(const RendererResourceName& name, const RayTracingPipelineShaders& shaders, const rhi::RayTracingPipelineDefinition& pipelineDef)
{
	const PipelineStateID pipelineID = ResourcesManager::GeneratePipelineID(shaders, pipelineDef);
	if (!m_registeredPipelines.contains(pipelineID))
	{
		m_rayTracingPSOsRequests.emplace_back(name, shaders, pipelineDef);
		m_registeredPipelines.insert(pipelineID);
	}
	return pipelineID;
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

	SPT_LOG_INFO(PSOsLibrary, "Precaching complete: {} shaders, {} compute PSOs, {} graphics PSOs, {} ray tracing PSOs",
				 m_shaderCompilationRequests.size(),
				 m_computePSOsRequests.size(),
				 m_graphicsPSOsRequests.size(),
				 m_rayTracingPSOsRequests.size());
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