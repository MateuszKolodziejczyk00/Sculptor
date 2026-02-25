#include "PipelinesCache.h"
#include "GPUApi.h"
#include "Shaders/ShadersManager.h"
#include "ResourcesManager.h"

namespace spt::rdr
{

SPT_DEFINE_LOG_CATEGORY(PipelinesCache, true)

#if WITH_SHADERS_HOT_RELOAD
#define READ_LOCK_IF_WITH_HOT_RELOAD const lib::ReadLockGuard hotReloadLockGuard(m_hotReloadLock);
#else
#define READ_LOCK_IF_WITH_HOT_RELOAD
#endif // WITH_SHADERS_HOT_RELOAD

PipelinesCache::PipelinesCache() = default;

PipelinesCache::~PipelinesCache() = default;

void PipelinesCache::Initialize()
{
	m_cachedGraphicsPipelines.reserve(1024);
	m_cachedComputePipelines.reserve(1024);
}

void PipelinesCache::Uninitialize()
{
	SPT_PROFILER_FUNCTION();

	ClearCachedPipelines();
}

PipelineStateID PipelinesCache::GeneratePipelineID(const GraphicsPipelineShaders& shaders, const rhi::GraphicsPipelineDefinition& pipelineDef) const
{
	return PipelineStateID(lib::HashCombine(shaders.Hash(), pipelineDef), rhi::EPipelineType::Graphics);
}

PipelineStateID PipelinesCache::GeneratePipelineID(const ShaderID& shader) const
{
	return PipelineStateID(lib::GetHash(shader), rhi::EPipelineType::Compute);
}

PipelineStateID PipelinesCache::GeneratePipelineID(const RayTracingPipelineShaders& shaders, const rhi::RayTracingPipelineDefinition& pipelineDef) const
{
	return PipelineStateID(lib::HashCombine(shaders.Hash(), pipelineDef), rhi::EPipelineType::RayTracing);
}

PipelineStateID PipelinesCache::GetOrCreateGfxPipeline(const RendererResourceName& nameInNotCached, const GraphicsPipelineShaders& shaders, const rhi::GraphicsPipelineDefinition& pipelineDef)
{
	const PipelineStateID stateID = GeneratePipelineID(shaders, pipelineDef);

	READ_LOCK_IF_WITH_HOT_RELOAD

	if (!m_cachedGraphicsPipelines.contains(stateID))
	{
		Bool shouldThisThreadCreatePipeline = false;

		{
			const lib::LockGuard lockGuard(m_graphicsPipelinesPendingFlushLock);

			const auto [pipelineIt, wasEmplaced] = m_graphicsPipelinesPendingFlush.try_emplace(stateID);

			shouldThisThreadCreatePipeline = wasEmplaced;
		}

		if (shouldThisThreadCreatePipeline)
		{
			lib::SharedPtr<GraphicsPipeline> pipeline = CreateGfxPipelineObject(nameInNotCached, shaders, pipelineDef);

			{
				lib::LockGuard lockGuard(m_graphicsPipelinesPendingFlushLock);
				m_graphicsPipelinesPendingFlush[stateID] = pipeline;

#if WITH_SHADERS_HOT_RELOAD
				GfxPipelineHotReloadData hotReloadData;
				hotReloadData.shaders                    = shaders;
				hotReloadData.pipelineDef                = pipelineDef;
				m_graphicsPipelineHotReloadData[stateID] = hotReloadData;

				if (shaders.vertexShader.IsValid())
				{
					m_shaderToPipelineStates[shaders.vertexShader].emplace_back(stateID);
				}
				if (shaders.taskShader.IsValid())
				{
					m_shaderToPipelineStates[shaders.taskShader].emplace_back(stateID);
				}
				if (shaders.meshShader.IsValid())
				{
					m_shaderToPipelineStates[shaders.meshShader].emplace_back(stateID);
				}
				if (shaders.fragmentShader.IsValid())
				{
					m_shaderToPipelineStates[shaders.fragmentShader].emplace_back(stateID);
				}
#endif // WITH_SHADERS_HOT_RELOAD
			}
		}
	}

	return stateID;
}

PipelineStateID PipelinesCache::GetOrCreateComputePipeline(const RendererResourceName& nameInNotCached, const ShaderID& shader)
{
	SPT_CHECK(shader.IsValid());

	const PipelineStateID stateID = GeneratePipelineID(shader);

	READ_LOCK_IF_WITH_HOT_RELOAD

	if (!m_cachedComputePipelines.contains(stateID))
	{
		Bool shouldThisThreadCreatePipeline = false;

		{
			const lib::LockGuard lockGuard(m_computePipelinesPendingFlushLock);

			const auto [pipelineIt, wasEmplaced] = m_computePipelinesPendingFlush.try_emplace(stateID);

			shouldThisThreadCreatePipeline = wasEmplaced;
		}

		if (shouldThisThreadCreatePipeline)
		{
			lib::SharedPtr<ComputePipeline> pipeline = CreateComputePipelineObject(nameInNotCached, shader);

			{
				const lib::LockGuard lockGuard(m_computePipelinesPendingFlushLock);
				m_computePipelinesPendingFlush[stateID] = pipeline;

#if WITH_SHADERS_HOT_RELOAD
				ComputePipelineHotReloadData hotReloadData;
				hotReloadData.shader = shader;
				m_computePipelineHotReloadData[stateID] = hotReloadData;

				m_shaderToPipelineStates[shader].emplace_back(stateID);
#endif // WITH_SHADERS_HOT_RELOAD
			}
		}
	}

	return stateID;
}

PipelineStateID PipelinesCache::GetOrCreateRayTracingPipeline(const RendererResourceName& nameInNotCached, const RayTracingPipelineShaders& shaders, const rhi::RayTracingPipelineDefinition& pipelineDef)
{
	SPT_CHECK(shaders.rayGenShader.IsValid());

	const PipelineStateID stateID = GeneratePipelineID(shaders, pipelineDef);

	READ_LOCK_IF_WITH_HOT_RELOAD

	if (!m_cachedRayTracingPipelines.contains(stateID))
	{
		Bool shouldThisThreadCreatePipeline = false;

		{
			const lib::LockGuard lockGuard(m_rayTracingPipelinesPendingFlushLock);

			const auto [pipelineIt, wasEmplaced] = m_rayTracingPipelinesPendingFlush.try_emplace(stateID);

			shouldThisThreadCreatePipeline = wasEmplaced;
		}

		if (shouldThisThreadCreatePipeline)
		{
			lib::SharedPtr<RayTracingPipeline> pipeline = CreateRayTracingPipelineObject(nameInNotCached, shaders, pipelineDef);

			{
				lib::LockGuard lockGuard(m_rayTracingPipelinesPendingFlushLock);
				m_rayTracingPipelinesPendingFlush[stateID] = pipeline;

#if WITH_SHADERS_HOT_RELOAD
				RayTracingPipelineHotReloadData hotReloadData;
				hotReloadData.shaders                      = shaders;
				hotReloadData.pipelineDef                  = pipelineDef;
				m_rayTracingPipelineHotReloadData[stateID] = hotReloadData;

				m_shaderToPipelineStates[shaders.rayGenShader].emplace_back(stateID);
				for (const RayTracingHitGroup& group : shaders.hitGroups)
				{
					if (group.closestHitShader.IsValid())
					{
						m_shaderToPipelineStates[group.closestHitShader].emplace_back(stateID);
					}

					if (group.anyHitShader.IsValid())
					{
						m_shaderToPipelineStates[group.anyHitShader].emplace_back(stateID);
					}

					if (group.intersectionShader.IsValid())
					{
						m_shaderToPipelineStates[group.intersectionShader].emplace_back(stateID);
					}
				}
				for (ShaderID shaderID : shaders.missShaders)
				{
					m_shaderToPipelineStates[shaderID].emplace_back(stateID);
				}
			}
#endif // WITH_SHADERS_HOT_RELOAD
		}
	}

	return stateID;
}

lib::SharedPtr<GraphicsPipeline> PipelinesCache::GetGraphicsPipeline(PipelineStateID id) const
{
	READ_LOCK_IF_WITH_HOT_RELOAD

	return GetPipelineImpl<GraphicsPipeline>(id, m_cachedGraphicsPipelines, m_graphicsPipelinesPendingFlush, m_graphicsPipelinesPendingFlushLock);
}

lib::SharedPtr<ComputePipeline> PipelinesCache::GetComputePipeline(PipelineStateID id) const
{
	return GetPipelineImpl<ComputePipeline>(id, m_cachedComputePipelines, m_computePipelinesPendingFlush, m_computePipelinesPendingFlushLock);
}

lib::SharedPtr<RayTracingPipeline> PipelinesCache::GetRayTracingPipeline(PipelineStateID id) const
{
	return GetPipelineImpl<RayTracingPipeline>(id, m_cachedRayTracingPipelines, m_rayTracingPipelinesPendingFlush, m_rayTracingPipelinesPendingFlushLock);
}

lib::SharedPtr<Pipeline> PipelinesCache::GetPipeline(PipelineStateID id) const
{
	switch (id.GetPipelineType())
	{
	case rhi::EPipelineType::Graphics:
		return GetGraphicsPipeline(id);
	case rhi::EPipelineType::Compute:
		return GetComputePipeline(id);
	case rhi::EPipelineType::RayTracing:
		return GetRayTracingPipeline(id);
	default:
		return nullptr;
	}}

void PipelinesCache::FlushCreatedPipelines()
{
	SPT_PROFILER_FUNCTION();

	FlushPipelinesImpl<GraphicsPipeline>(m_cachedGraphicsPipelines, m_graphicsPipelinesPendingFlush, m_graphicsPipelinesPendingFlushLock);
	FlushPipelinesImpl<ComputePipeline>(m_cachedComputePipelines, m_computePipelinesPendingFlush, m_computePipelinesPendingFlushLock);
	FlushPipelinesImpl<RayTracingPipeline>(m_cachedRayTracingPipelines, m_rayTracingPipelinesPendingFlush, m_rayTracingPipelinesPendingFlushLock);

#if WITH_SHADERS_HOT_RELOAD
	FlushPipelinesHotReloads();
#endif // WITH_SHADERS_HOT_RELOAD
}

void PipelinesCache::ClearCachedPipelines()
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
void PipelinesCache::InvalidatePipelinesUsingShaders(lib::Span<const ShaderID> shaders)
{
	SPT_PROFILER_FUNCTION();

	const lib::LockGuard invalidatedShadersLock(m_invalidatedShadersLock);

	m_invalidatedShaders.reserve(m_invalidatedShaders.size() + shaders.size());
	for (ShaderID shader : shaders)
	{
		m_invalidatedShaders.emplace_back(shader);
	}
}
#endif // WITH_SHADERS_HOT_RELOAD

lib::SharedRef<GraphicsPipeline> PipelinesCache::CreateGfxPipelineObject(const RendererResourceName& nameInNotCached, const GraphicsPipelineShaders& shaders, const rhi::GraphicsPipelineDefinition& pipelineDef)
{
	GraphicsPipelineShadersDefinition shadersDef;

	SPT_CHECK(shaders.vertexShader.IsValid() || shaders.meshShader.IsValid());

	if (shaders.vertexShader.IsValid())
	{
		shadersDef.vertexShader = GPUApi::GetShadersManager().GetShader(shaders.vertexShader);
	}

	if (shaders.taskShader.IsValid())
	{
		shadersDef.taskShader = GPUApi::GetShadersManager().GetShader(shaders.taskShader);
	}

	if (shaders.meshShader.IsValid())
	{
		shadersDef.meshShader = GPUApi::GetShadersManager().GetShader(shaders.meshShader);
	}

	SPT_CHECK(shaders.fragmentShader.IsValid());
	shadersDef.fragmentShader	= GPUApi::GetShadersManager().GetShader(shaders.fragmentShader);

	return ResourcesManager::CreateGfxPipelineObject(nameInNotCached, shadersDef, pipelineDef);
}

lib::SharedRef<ComputePipeline> PipelinesCache::CreateComputePipelineObject(const RendererResourceName& nameInNotCached, const ShaderID& shader)
{
	const lib::SharedRef<Shader> shaderObject = GPUApi::GetShadersManager().GetShader(shader);
	return ResourcesManager::CreateComputePipelineObject(nameInNotCached, shaderObject);
}

lib::SharedRef<RayTracingPipeline> PipelinesCache::CreateRayTracingPipelineObject(const RendererResourceName& nameInNotCached, const RayTracingPipelineShaders& shaders, const rhi::RayTracingPipelineDefinition& pipelineDef)
{
	const auto getShaderObject = [](ShaderID shaderID)
	{
		return GPUApi::GetShadersManager().GetShader(shaderID);
	};

	RayTracingPipelineShaderObjects shadersObjects;
	shadersObjects.hitGroups.reserve(shaders.hitGroups.size());
	shadersObjects.missShaders.reserve(shaders.missShaders.size());

	shadersObjects.rayGenShader = getShaderObject(shaders.rayGenShader);

	std::transform(std::cbegin(shaders.hitGroups), std::cend(shaders.hitGroups),
				   std::back_inserter(shadersObjects.hitGroups),
				   [getShaderObject](const RayTracingHitGroup& hitGroup)
				   {
					   RayTracingHitGroupShaders hitGroupShaders;
					   if (hitGroup.closestHitShader.IsValid())
					   {
						   hitGroupShaders.closestHitShader = getShaderObject(hitGroup.closestHitShader);
					   }
					   if (hitGroup.anyHitShader.IsValid())
					   {
						   hitGroupShaders.anyHitShader = getShaderObject(hitGroup.anyHitShader);
					   }
					   if (hitGroup.intersectionShader.IsValid())
					   {
						   hitGroupShaders.intersectionShader = getShaderObject(hitGroup.intersectionShader);
					   }
					   return hitGroupShaders;
				   });
	
	std::transform(std::cbegin(shaders.missShaders), std::cend(shaders.missShaders),
				   std::back_inserter(shadersObjects.missShaders),
				   getShaderObject);

	return ResourcesManager::CreateRayTracingPipelineObject(nameInNotCached, shadersObjects, pipelineDef);
}

#if WITH_SHADERS_HOT_RELOAD
void PipelinesCache::FlushPipelinesHotReloads()
{
	const lib::LockGuard invalidatedShadersLock(m_invalidatedShadersLock);
	const lib::WriteLockGuard hotReloadWriteGuard(m_hotReloadLock);

	lib::DynamicArray<PipelineStateID> invalidatedPipelines;

	for (ShaderID shader : m_invalidatedShaders)
	{
		const lib::SharedRef<Shader> shaderObject = GPUApi::GetShadersManager().GetShader(shader);
		const auto pipelinesToInvalidate = m_shaderToPipelineStates.find(shader);

		if (pipelinesToInvalidate != std::cend(m_shaderToPipelineStates))
		{
			for (PipelineStateID pipelineID : pipelinesToInvalidate->second)
			{
				if (!lib::Contains(invalidatedPipelines, pipelineID))
				{
					invalidatedPipelines.emplace_back(pipelineID);
				}
			}
		}
	}

	for (PipelineStateID pipelineID : invalidatedPipelines)
	{
		const auto computePipelineIt = m_cachedComputePipelines.find(pipelineID);
		if (computePipelineIt != std::cend(m_cachedComputePipelines))
		{
			const ComputePipelineHotReloadData& hotReloadData = m_computePipelineHotReloadData[pipelineID];

			lib::SharedPtr<ComputePipeline>& computePipeline = computePipelineIt->second;
			computePipeline = CreateComputePipelineObject(RENDERER_RESOURCE_NAME(computePipeline->GetRHI().GetName()), hotReloadData.shader);

			SPT_LOG_INFO(PipelinesCache, "Hot reloaded compute pipeline: {}", computePipeline->GetRHI().GetName().GetData());
		}
		else
		{
			const auto graphicsPipelineIt = m_cachedGraphicsPipelines.find(pipelineID);
			if (graphicsPipelineIt != std::cend(m_cachedGraphicsPipelines))
			{
				const GfxPipelineHotReloadData& hotReloadData = m_graphicsPipelineHotReloadData[pipelineID];

				lib::SharedPtr<GraphicsPipeline>& graphicsPipeline = graphicsPipelineIt->second;
				graphicsPipeline = CreateGfxPipelineObject(RENDERER_RESOURCE_NAME(graphicsPipeline->GetRHI().GetName()), hotReloadData.shaders, hotReloadData.pipelineDef);
			
				SPT_LOG_INFO(PipelinesCache, "Hot reloaded graphics pipeline: {}", graphicsPipeline->GetRHI().GetName().GetData());
			}
			else
			{
				const RayTracingPipelineHotReloadData& hotReloadData = m_rayTracingPipelineHotReloadData[pipelineID];

				lib::SharedPtr<RayTracingPipeline>& rayTracingPipeline = m_cachedRayTracingPipelines[pipelineID];
				rayTracingPipeline = CreateRayTracingPipelineObject(RENDERER_RESOURCE_NAME(rayTracingPipeline->GetRHI().GetName()), hotReloadData.shaders, hotReloadData.pipelineDef);
			
				SPT_LOG_INFO(PipelinesCache, "Hot reloaded ray tracing pipeline: {}", rayTracingPipeline->GetRHI().GetName().GetData());
			}
		}
	}

	m_invalidatedShaders.clear();
}
#endif // WITH_SHADERS_HOT_RELOAD

} // spt::rdr
