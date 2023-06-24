#include "PipelinesLibrary.h"
#include "Renderer.h"
#include "Shaders/ShadersManager.h"

namespace spt::rdr
{

#if WITH_SHADERS_HOT_RELOAD
#define READ_LOCK_IF_WITH_HOT_RELOAD const lib::ReadLockGuard hotReloadLockGuard(m_hotReloadLock);
#else
#define READ_LOCK_IF_WITH_HOT_RELOAD
#endif // WITH_SHADERS_HOT_RELOAD

PipelinesLibrary::PipelinesLibrary() = default;

PipelinesLibrary::~PipelinesLibrary() = default;

void PipelinesLibrary::Initialize()
{
	m_cachedGraphicsPipelines.reserve(1024);
	m_cachedComputePipelines.reserve(1024);
}

void PipelinesLibrary::Uninitialize()
{
	SPT_PROFILER_FUNCTION();

	ClearCachedPipelines();
}

PipelineStateID PipelinesLibrary::GetOrCreateGfxPipeline(const RendererResourceName& nameInNotCached, const GraphicsPipelineShaders& shaders, const rhi::GraphicsPipelineDefinition& pipelineDef)
{
	SPT_PROFILER_FUNCTION();

	const PipelineStateID stateID = GetStateID(shaders, pipelineDef);

	READ_LOCK_IF_WITH_HOT_RELOAD

	if (!m_cachedGraphicsPipelines.contains(stateID))
	{
		const lib::LockGuard lockGuard(m_graphicsPipelinesPendingFlushLock);

		lib::SharedPtr<GraphicsPipeline>& pendingPipeline = m_graphicsPipelinesPendingFlush[stateID];

		if (!pendingPipeline)
		{
			pendingPipeline = CreateGfxPipelineObject(nameInNotCached, shaders, pipelineDef);

#if WITH_SHADERS_HOT_RELOAD
			GfxPipelineHotReloadData hotReloadData;
			hotReloadData.shaders		= shaders;
			hotReloadData.pipelineDef	= pipelineDef;
			m_graphicsPipelineHotReloadData[stateID] = hotReloadData;

			m_shaderToPipelineStates[shaders.vertexShader].emplace_back(stateID);
			m_shaderToPipelineStates[shaders.vertexShader].emplace_back(stateID);
#endif // WITH_SHADERS_HOT_RELOAD
		}
	}

	return stateID;
}

PipelineStateID PipelinesLibrary::GetOrCreateComputePipeline(const RendererResourceName& nameInNotCached, const ShaderID& shader)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(shader.IsValid());

	const PipelineStateID stateID = GetStateID(shader);

	READ_LOCK_IF_WITH_HOT_RELOAD

	if (!m_cachedComputePipelines.contains(stateID))
	{
		const lib::LockGuard lockGuard(m_computePipelinesPendingFlushLock);

		lib::SharedPtr<ComputePipeline>& pendingPipeline = m_computePipelinesPendingFlush[stateID];

		if (!pendingPipeline)
		{
			pendingPipeline = CreateComputePipelineObject(nameInNotCached, shader);

#if WITH_SHADERS_HOT_RELOAD
			m_shaderToPipelineStates[shader].emplace_back(stateID);
#endif // WITH_SHADERS_HOT_RELOAD
		}
	}

	return stateID;
}

PipelineStateID PipelinesLibrary::GetOrCreateRayTracingPipeline(const RendererResourceName& nameInNotCached, const RayTracingPipelineShaders& shaders, const rhi::RayTracingPipelineDefinition& pipelineDef)
{
	SPT_PROFILER_FUNCTION();

	const PipelineStateID stateID = GetStateID(shaders, pipelineDef);

	READ_LOCK_IF_WITH_HOT_RELOAD

	if (!m_cachedRayTracingPipelines.contains(stateID))
	{
		const lib::LockGuard lockGuard(m_rayTracingPipelinesPendingFlushLock);

		lib::SharedPtr<RayTracingPipeline>& pendingPipeline = m_rayTracingPipelinesPendingFlush[stateID];

		if (!pendingPipeline)
		{
			pendingPipeline = CreateRayTracingPipelineObject(nameInNotCached, shaders, pipelineDef);
				 
#if WITH_SHADERS_HOT_RELOAD
			RayTracingPipelineHotReloadData hotReloadData;
			hotReloadData.shaders		= shaders;
			hotReloadData.pipelineDef	= pipelineDef;
			m_rayTracingPipelineHotReloadData[stateID] = hotReloadData;

			for (ShaderID shaderID : shaders.shaders)
			{
				m_shaderToPipelineStates[shaderID].emplace_back(stateID);
			}
#endif // WITH_SHADERS_HOT_RELOAD
		}
	}

	return stateID;
}

lib::SharedPtr<GraphicsPipeline> PipelinesLibrary::GetGraphicsPipeline(PipelineStateID id) const
{
	READ_LOCK_IF_WITH_HOT_RELOAD

	return GetPipelineImpl<GraphicsPipeline>(id, m_cachedGraphicsPipelines, m_graphicsPipelinesPendingFlush, m_graphicsPipelinesPendingFlushLock);
}

lib::SharedPtr<ComputePipeline> PipelinesLibrary::GetComputePipeline(PipelineStateID id) const
{
	return GetPipelineImpl<ComputePipeline>(id, m_cachedComputePipelines, m_computePipelinesPendingFlush, m_computePipelinesPendingFlushLock);
}

lib::SharedPtr<RayTracingPipeline> PipelinesLibrary::GetRayTracingPipeline(PipelineStateID id) const
{
	return GetPipelineImpl<RayTracingPipeline>(id, m_cachedRayTracingPipelines, m_rayTracingPipelinesPendingFlush, m_rayTracingPipelinesPendingFlushLock);
}

void PipelinesLibrary::FlushCreatedPipelines()
{
	SPT_PROFILER_FUNCTION();

	FlushPipelinesImpl<GraphicsPipeline>(m_cachedGraphicsPipelines, m_graphicsPipelinesPendingFlush, m_graphicsPipelinesPendingFlushLock);
	FlushPipelinesImpl<ComputePipeline>(m_cachedComputePipelines, m_computePipelinesPendingFlush, m_computePipelinesPendingFlushLock);
	FlushPipelinesImpl<RayTracingPipeline>(m_cachedRayTracingPipelines, m_rayTracingPipelinesPendingFlush, m_rayTracingPipelinesPendingFlushLock);

#if WITH_SHADERS_HOT_RELOAD
	FlushPipelinesHotReloads();
#endif // WITH_SHADERS_HOT_RELOAD
}

void PipelinesLibrary::ClearCachedPipelines()
{
	SPT_PROFILER_FUNCTION();

	READ_LOCK_IF_WITH_HOT_RELOAD

	m_cachedGraphicsPipelines.clear();
	m_cachedComputePipelines.clear();
	m_cachedRayTracingPipelines.clear();

	{
		const lib::LockGuard lockGuard(m_graphicsPipelinesPendingFlushLock);
		m_graphicsPipelinesPendingFlush.clear();
	}
	
	{
		const lib::LockGuard lockGuard(m_computePipelinesPendingFlushLock);
		m_computePipelinesPendingFlush.clear();
	}

	{
		const lib::LockGuard lockGuard(m_rayTracingPipelinesPendingFlushLock);
		m_rayTracingPipelinesPendingFlush.clear();
	}
}

#if WITH_SHADERS_HOT_RELOAD
void PipelinesLibrary::InvalidatePipelinesUsingShader(ShaderID shader)
{
	SPT_PROFILER_FUNCTION();

	const lib::LockGuard invalidatedShadersLock(m_invalidatedShadersLock);
	m_invalidatedShaders.emplace_back(shader);
}
#endif // WITH_SHADERS_HOT_RELOAD

PipelineStateID PipelinesLibrary::GetStateID(const GraphicsPipelineShaders& shaders, const rhi::GraphicsPipelineDefinition& pipelineDef) const
{
	SPT_PROFILER_FUNCTION();

	return PipelineStateID(lib::HashCombine(shaders.Hash(), pipelineDef));
}

PipelineStateID PipelinesLibrary::GetStateID(const ShaderID& shader) const
{
	SPT_PROFILER_FUNCTION();

	return PipelineStateID(lib::GetHash(shader));
}

PipelineStateID PipelinesLibrary::GetStateID(const RayTracingPipelineShaders& shaders, const rhi::RayTracingPipelineDefinition& pipelineDef) const
{
	SPT_PROFILER_FUNCTION();

	return PipelineStateID(lib::HashCombine(shaders.Hash(), pipelineDef));
}

lib::SharedRef<GraphicsPipeline> PipelinesLibrary::CreateGfxPipelineObject(const RendererResourceName& nameInNotCached, const GraphicsPipelineShaders& shaders, const rhi::GraphicsPipelineDefinition& pipelineDef)
{
	GraphicsPipelineShadersDefinition shadersDef;
	shadersDef.vertexShader		= Renderer::GetShadersManager().GetShader(shaders.vertexShader);
	shadersDef.fragmentShader	= Renderer::GetShadersManager().GetShader(shaders.fragmentShader);
	return lib::MakeShared<GraphicsPipeline>(nameInNotCached, shadersDef, pipelineDef);
}

lib::SharedRef<ComputePipeline> PipelinesLibrary::CreateComputePipelineObject(const RendererResourceName& nameInNotCached, const ShaderID& shader)
{
	const lib::SharedRef<Shader> shaderObject = Renderer::GetShadersManager().GetShader(shader);
	return lib::MakeShared<ComputePipeline>(nameInNotCached, shaderObject);
}

lib::SharedRef<RayTracingPipeline> PipelinesLibrary::CreateRayTracingPipelineObject(const RendererResourceName& nameInNotCached, const RayTracingPipelineShaders& shaders, const rhi::RayTracingPipelineDefinition& pipelineDef)
{
	lib::DynamicArray<lib::SharedRef<Shader>> shaderObjects;
	shaderObjects.reserve(shaders.shaders.size());
	std::transform(std::cbegin(shaders.shaders), std::cend(shaders.shaders),
				   std::back_inserter(shaderObjects),
				   [](ShaderID shaderID)
				   {
					   return Renderer::GetShadersManager().GetShader(shaderID);
				   });

	return lib::MakeShared<RayTracingPipeline>(nameInNotCached, shaderObjects, pipelineDef);
}

#if WITH_SHADERS_HOT_RELOAD
void PipelinesLibrary::FlushPipelinesHotReloads()
{
	const lib::LockGuard invalidatedShadersLock(m_invalidatedShadersLock);
	const lib::WriteLockGuard hotReloadWriteGuard(m_hotReloadLock);

	for (ShaderID shader : m_invalidatedShaders)
	{
		const lib::SharedRef<Shader> shaderObject = Renderer::GetShadersManager().GetShader(shader);
		const auto pipelinesToInvalidate = m_shaderToPipelineStates.find(shader);
		if (pipelinesToInvalidate != std::cend(m_shaderToPipelineStates))
		{
			for (PipelineStateID pipelineID : pipelinesToInvalidate->second)
			{
				const auto computePipelineIt = m_cachedComputePipelines.find(pipelineID);
				if (computePipelineIt != std::cend(m_cachedComputePipelines))
				{
					lib::SharedPtr<ComputePipeline>& computePipeline = computePipelineIt->second;
					computePipeline = CreateComputePipelineObject(RENDERER_RESOURCE_NAME(computePipeline->GetRHI().GetName()), shader);

				}
				else
				{
					const auto graphicsPipelineIt = m_cachedGraphicsPipelines.find(pipelineID);
					if (graphicsPipelineIt != std::cend(m_cachedGraphicsPipelines))
					{
						const GfxPipelineHotReloadData& hotReloadData = m_graphicsPipelineHotReloadData[pipelineID];

						lib::SharedPtr<GraphicsPipeline>& graphicsPipeline = graphicsPipelineIt->second;
						graphicsPipeline = CreateGfxPipelineObject(RENDERER_RESOURCE_NAME(graphicsPipeline->GetRHI().GetName()), hotReloadData.shaders, hotReloadData.pipelineDef);
					}
					else
					{
						const RayTracingPipelineHotReloadData& hotReloadData = m_rayTracingPipelineHotReloadData[pipelineID];

						lib::SharedPtr<RayTracingPipeline>& rayTracingPipeline = m_cachedRayTracingPipelines[pipelineID];
						rayTracingPipeline = CreateRayTracingPipelineObject(RENDERER_RESOURCE_NAME(rayTracingPipeline->GetRHI().GetName()), hotReloadData.shaders, hotReloadData.pipelineDef);
					}
				}
			}
		}
	}

	m_invalidatedShaders.clear();
}
#endif // WITH_SHADERS_HOT_RELOAD

} // spt::rdr
