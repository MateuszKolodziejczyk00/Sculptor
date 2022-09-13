#pragma once

#include "SculptorCoreTypes.h"
#include "PipelineState.h"


namespace spt::rdr
{

class Context;
class Pipeline;


class PipelinesLibrary
{
public:

	PipelinesLibrary();

	// Define in header, so we don't have to include Pipeline
	~PipelinesLibrary();

	void Initialize();
	void Uninitialize();

	SPT_NODISCARD PipelineStateID GetOrCreateGfxPipeline(const lib::SharedRef<Context>& context, const rhi::GraphicsPipelineDefinition& pipelineDef, const ShaderID& shader);
	SPT_NODISCARD PipelineStateID GetOrCreateComputePipeline(const lib::SharedRef<Context>& context, const ShaderID& shader);
	
	SPT_NODISCARD lib::SharedPtr<Pipeline> GetPipeline(PipelineStateID id) const;

	void FlushCreatedPipelines();

	void ClearCachedPipelines();
	
private:

	PipelineStateID GetStateID(const rhi::GraphicsPipelineDefinition& pipelineDef, const ShaderID& shader) const;
	PipelineStateID GetStateID(const ShaderID& shader) const;

	/**
	 * These are pipelines created in previous frames
	 * They can be modified only during FlushCreatedPipelines, which shouldn't be called async with rendering threads
	 * Because of that it provides fast, threadsafe access without locks during recording command buffers
	 */
	lib::HashMap<PipelineStateID, lib::SharedRef<Pipeline>> m_cachedPipelines;

	/**
	 * These are pipelines created during current frame
	 * Access to these pipelines is slower, because it has to be synchronized using lock (as other threads may add newly created pipelines)
	 * During FlushCreatedPipelines this map is copied to m_cachedPipelines and cleared
	 */
	lib::HashMap<PipelineStateID, lib::SharedRef<Pipeline>> m_pipelinesPendingFlush;
	lib::Lock	m_pipelinesPendingFlushLock;
};



} // spt::rdr