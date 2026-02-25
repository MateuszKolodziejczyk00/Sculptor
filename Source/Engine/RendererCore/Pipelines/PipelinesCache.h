#pragma once

#include "RendererCoreMacros.h"
#include "SculptorCoreTypes.h"
#include "Types/Pipeline/GraphicsPipeline.h"
#include "Types/Pipeline/ComputePipeline.h"
#include "Types/Pipeline/RayTracingPipeline.h"
#include "PipelineState.h"
#include "JobSystem.h"


namespace spt::rdr
{

class PipelinesCache
{
public:

	PipelinesCache();

	// Define in header, so we don't have to include Pipeline
	~PipelinesCache();

	void Initialize();
	void Uninitialize();

	PipelineStateID GeneratePipelineID(const GraphicsPipelineShaders& shaders, const rhi::GraphicsPipelineDefinition& pipelineDef) const;
	PipelineStateID GeneratePipelineID(const ShaderID& shader) const;
	PipelineStateID GeneratePipelineID(const RayTracingPipelineShaders& shaders, const rhi::RayTracingPipelineDefinition& pipelineDef) const;

	SPT_NODISCARD PipelineStateID GetOrCreateGfxPipeline(const RendererResourceName& nameInNotCached, const GraphicsPipelineShaders& shaders, const rhi::GraphicsPipelineDefinition& pipelineDef);
	SPT_NODISCARD PipelineStateID GetOrCreateComputePipeline(const RendererResourceName& nameInNotCached, const ShaderID& shader);
	SPT_NODISCARD PipelineStateID GetOrCreateRayTracingPipeline(const RendererResourceName& nameInNotCached, const RayTracingPipelineShaders& shaders, const rhi::RayTracingPipelineDefinition& pipelineDef);
	
	SPT_NODISCARD lib::SharedPtr<GraphicsPipeline>   GetGraphicsPipeline(PipelineStateID id) const;
	
	SPT_NODISCARD lib::SharedPtr<ComputePipeline>    GetComputePipeline(PipelineStateID id) const;
	SPT_NODISCARD lib::SharedPtr<RayTracingPipeline> GetRayTracingPipeline(PipelineStateID id) const;

	SPT_NODISCARD lib::SharedPtr<Pipeline> GetPipeline(PipelineStateID id) const;

	void FlushCreatedPipelines();

	/** Shouldn't be called during rendering, as it modifies cached pipelines maps without any synchronization */
	void ClearCachedPipelines();

#if WITH_SHADERS_HOT_RELOAD
	void InvalidatePipelinesUsingShaders(lib::Span<const ShaderID> shaders);
#endif // WITH_SHADERS_HOT_RELOAD
	
private:

	template<typename TPipelineType>
	using PipelinesMap = lib::HashMap<PipelineStateID, lib::SharedPtr<TPipelineType>>;

	SPT_NODISCARD lib::SharedRef<GraphicsPipeline>		CreateGfxPipelineObject(const RendererResourceName& nameInNotCached, const GraphicsPipelineShaders& shaders, const rhi::GraphicsPipelineDefinition& pipelineDef);
	SPT_NODISCARD lib::SharedRef<ComputePipeline>		CreateComputePipelineObject(const RendererResourceName& nameInNotCached, const ShaderID& shader);
	SPT_NODISCARD lib::SharedRef<RayTracingPipeline>	CreateRayTracingPipelineObject(const RendererResourceName& nameInNotCached, const RayTracingPipelineShaders& shaders, const rhi::RayTracingPipelineDefinition& pipelineDef);

	template<typename TPipelineType>
	SPT_NODISCARD lib::SharedPtr<TPipelineType> GetPipelineImpl(PipelineStateID id, const PipelinesMap<TPipelineType>& cachedPipelines, const PipelinesMap<TPipelineType>& pipelinesPendingFlush, lib::Lock& lock) const;

	template<typename TPipelineType>
	void FlushPipelinesImpl(PipelinesMap<TPipelineType>& cachedPipelines, PipelinesMap<TPipelineType>& pipelinesPendingFlush, lib::Lock& lock);

#if WITH_SHADERS_HOT_RELOAD
	void FlushPipelinesHotReloads();
#endif // WITH_SHADERS_HOT_RELOAD

	/**
	 * These are pipelines created in previous frames
	 * They can be modified only during FlushCreatedPipelines, which shouldn't be called async with rendering threads
	 * Because of that it provides fast, threadsafe access without locks during recording command buffers
	 */
	PipelinesMap<GraphicsPipeline>		m_cachedGraphicsPipelines;
	PipelinesMap<ComputePipeline>		m_cachedComputePipelines;
	PipelinesMap<RayTracingPipeline>	m_cachedRayTracingPipelines;

	/**
	 * These are pipelines created during current frame
	 * Access to these pipelines is slower, because it has to be synchronized using lock (as other threads may add newly created pipelines)
	 * During FlushCreatedPipelines this map is copied to m_cachedPipelines and cleared
	 */
	PipelinesMap<GraphicsPipeline>	m_graphicsPipelinesPendingFlush;
	mutable lib::Lock				m_graphicsPipelinesPendingFlushLock;

	PipelinesMap<ComputePipeline>	m_computePipelinesPendingFlush;
	mutable lib::Lock				m_computePipelinesPendingFlushLock;

	PipelinesMap<RayTracingPipeline>	m_rayTracingPipelinesPendingFlush;
	mutable lib::Lock					m_rayTracingPipelinesPendingFlushLock;

#if WITH_SHADERS_HOT_RELOAD
	struct ComputePipelineHotReloadData
	{
		ShaderID shader;
	};

	struct GfxPipelineHotReloadData
	{
		GraphicsPipelineShaders shaders;
		rhi::GraphicsPipelineDefinition pipelineDef;
	};

	struct RayTracingPipelineHotReloadData
	{
		RayTracingPipelineShaders shaders;
		rhi::RayTracingPipelineDefinition pipelineDef;
	};

	lib::HashMap<ShaderID, lib::DynamicArray<PipelineStateID>>		m_shaderToPipelineStates;
	lib::HashMap<PipelineStateID, ComputePipelineHotReloadData>		m_computePipelineHotReloadData;
	lib::HashMap<PipelineStateID, GfxPipelineHotReloadData>			m_graphicsPipelineHotReloadData;
	lib::HashMap<PipelineStateID, RayTracingPipelineHotReloadData>	m_rayTracingPipelineHotReloadData;
	mutable lib::ReadWriteLock										m_hotReloadLock;
	
	lib::DynamicArray<ShaderID>	m_invalidatedShaders;
	mutable lib::Lock			m_invalidatedShadersLock;
#endif // WITH_SHADERS_HOT_RELOAD
};


template<typename TPipelineType>
lib::SharedPtr<TPipelineType> PipelinesCache::GetPipelineImpl(PipelineStateID id, const PipelinesMap<TPipelineType>& cachedPipelines, const PipelinesMap<TPipelineType>& pipelinesPendingFlush, lib::Lock& lock) const
{
	SPT_CHECK(id.IsValid());

	const auto foundCachedPipeline = cachedPipelines.find(id);
	if (foundCachedPipeline != std::cend(cachedPipelines))
	{
		return foundCachedPipeline->second;
	}

	while (true)
	{
		{
			//before we start searching for pipeline in pipelines pending flush, we need to get exclusive access
			const lib::LockGuard lockGuard(lock);

			const auto foundPendingPipeline = pipelinesPendingFlush.find(id);

			if (foundPendingPipeline == std::cend(pipelinesPendingFlush))
			{
				return nullptr;
			}

			lib::SharedPtr<TPipelineType> pendingPipeline = foundPendingPipeline->second;
			if (pendingPipeline)
			{
				return pendingPipeline;
			}
		}

		// Another thread is still compiling this pipeline. Let's just active wait
		js::Worker::TryActiveWait();
	}
}

template<typename TPipelineType>
void PipelinesCache::FlushPipelinesImpl(PipelinesMap<TPipelineType>& cachedPipelines, PipelinesMap<TPipelineType>& pipelinesPendingFlush, lib::Lock& lock)
{
	SPT_PROFILER_FUNCTION();
	
	const lib::LockGuard lockGuard(lock);

	std::move(std::begin(pipelinesPendingFlush), std::end(pipelinesPendingFlush), std::inserter(cachedPipelines, std::end(cachedPipelines)));
	pipelinesPendingFlush.clear();
}

} // spt::rdr
