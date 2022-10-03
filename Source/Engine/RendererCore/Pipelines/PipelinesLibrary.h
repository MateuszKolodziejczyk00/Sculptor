#pragma once

#include "RendererCoreMacros.h"
#include "SculptorCoreTypes.h"
#include "Types/Pipeline/GraphicsPipeline.h"
#include "Types/Pipeline/ComputePipeline.h"
#include "PipelineState.h"


namespace spt::rdr
{

class PipelinesLibrary
{
public:

	PipelinesLibrary();

	// Define in header, so we don't have to include Pipeline
	~PipelinesLibrary();

	void Initialize();
	void Uninitialize();

	SPT_NODISCARD PipelineStateID GetOrCreateGfxPipeline(const RendererResourceName& nameInNotCached, const rhi::GraphicsPipelineDefinition& pipelineDef, const ShaderID& shader);
	SPT_NODISCARD PipelineStateID GetOrCreateComputePipeline(const RendererResourceName& nameInNotCached, const ShaderID& shader);
	
	SPT_NODISCARD lib::SharedRef<GraphicsPipeline> GetGraphicsPipeline(PipelineStateID id) const;
	SPT_NODISCARD lib::SharedRef<ComputePipeline> GetComputePipeline(PipelineStateID id) const;

	void FlushCreatedPipelines();

	/** Shouldn't be called during rendering, as it modifies cached pipelines maps without any synchronization */
	void ClearCachedPipelines();
	
private:

	template<typename TPipelineType>
	using PipelinesMap = lib::HashMap<PipelineStateID, lib::SharedPtr<TPipelineType>>;

	PipelineStateID GetStateID(const rhi::GraphicsPipelineDefinition& pipelineDef, const ShaderID& shader) const;
	PipelineStateID GetStateID(const ShaderID& shader) const;

	template<typename TPipelineType>
	SPT_NODISCARD lib::SharedRef<TPipelineType> GetPipelineImpl(PipelineStateID id, const PipelinesMap<TPipelineType>& cachedPipelines, const PipelinesMap<TPipelineType>& pipelinesPendingFlush, lib::Lock& lock) const;

	template<typename TPipelineType>
	void FlushPipelinesImpl(PipelinesMap<TPipelineType>& cachedPipelines, PipelinesMap<TPipelineType>& pipelinesPendingFlush, lib::Lock& lock);

	/**
	 * These are pipelines created in previous frames
	 * They can be modified only during FlushCreatedPipelines, which shouldn't be called async with rendering threads
	 * Because of that it provides fast, threadsafe access without locks during recording command buffers
	 */
	PipelinesMap<GraphicsPipeline>	m_cachedGraphicsPipelines;
	PipelinesMap<ComputePipeline>	m_cachedComputePipelines;

	/**
	 * These are pipelines created during current frame
	 * Access to these pipelines is slower, because it has to be synchronized using lock (as other threads may add newly created pipelines)
	 * During FlushCreatedPipelines this map is copied to m_cachedPipelines and cleared
	 */
	PipelinesMap<GraphicsPipeline>	m_graphicsPipelinesPendingFlush;
	mutable lib::Lock				m_graphicsPipelinesPendingFlushLock;

	PipelinesMap<ComputePipeline>	m_computePipelinesPendingFlush;
	mutable lib::Lock				m_computePipelinesPendingFlushLock;
};


template<typename TPipelineType>
lib::SharedRef<TPipelineType> PipelinesLibrary::GetPipelineImpl(PipelineStateID id, const PipelinesMap<TPipelineType>& cachedPipelines, const PipelinesMap<TPipelineType>& pipelinesPendingFlush, lib::Lock& lock) const
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(id.IsValid());

	const auto foundCachedPipeline = cachedPipelines.find(id);
	if (foundCachedPipeline != std::cend(cachedPipelines))
	{
		return lib::Ref(foundCachedPipeline->second);
	}

	//before we start searching for pipeline in pipelines pending flush, we need to get exclusive access
	const lib::LockGuard lockGuard(lock);

	const auto foundPendingPipeline = pipelinesPendingFlush.find(id);
	SPT_CHECK(foundPendingPipeline != std::cend(pipelinesPendingFlush));

	return lib::Ref(foundPendingPipeline->second);
}

template<typename TPipelineType>
void PipelinesLibrary::FlushPipelinesImpl(PipelinesMap<TPipelineType>& cachedPipelines, PipelinesMap<TPipelineType>& pipelinesPendingFlush, lib::Lock& lock)
{
	SPT_PROFILER_FUNCTION();
	
	const lib::LockGuard lockGuard(lock);

	std::move(std::begin(pipelinesPendingFlush), std::end(pipelinesPendingFlush), std::inserter(cachedPipelines, std::end(cachedPipelines)));
	pipelinesPendingFlush.clear();
}

} // spt::rdr