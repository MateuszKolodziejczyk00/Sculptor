#include "PipelinesLibrary.h"

namespace spt::rdr
{

PipelinesLibrary::PipelinesLibrary() = default;

PipelinesLibrary::~PipelinesLibrary() = default;

void PipelinesLibrary::Initialize()
{
	// Nothing for now
}

void PipelinesLibrary::Uninitialize()
{
	SPT_PROFILER_FUNCTION();

	ClearCachedPipelines();
}

PipelineStateID PipelinesLibrary::GetOrCreateGfxPipeline(const lib::SharedRef<Context>& context, const rhi::GraphicsPipelineDefinition& pipelineDef, const ShaderID& shader)
{
	SPT_PROFILER_FUNCTION();

	return 0;
}

PipelineStateID PipelinesLibrary::GetOrCreateComputePipeline(const lib::SharedRef<Context>& context, const ShaderID& shader)
{
	SPT_PROFILER_FUNCTION();

	return 0;
}

lib::SharedPtr<Pipeline> PipelinesLibrary::GetPipeline(PipelineStateID id) const
{
	return lib::SharedPtr<Pipeline>();
}

void PipelinesLibrary::FlushCreatedPipelines()
{
	SPT_PROFILER_FUNCTION();

	const lib::LockGuard lockGuard(m_pipelinesPendingFlushLock);

	std::move(std::begin(m_pipelinesPendingFlush), std::end(m_pipelinesPendingFlush), std::inserter(m_cachedPipelines, std::end(m_cachedPipelines)));

	m_pipelinesPendingFlush.clear();
}

void PipelinesLibrary::ClearCachedPipelines()
{
	SPT_PROFILER_FUNCTION();

	m_cachedPipelines.clear();

	{
		const lib::LockGuard lockGuard(m_pipelinesPendingFlushLock);
		m_pipelinesPendingFlush.clear();
	}
}

PipelineStateID PipelinesLibrary::GetStateID(const rhi::GraphicsPipelineDefinition& pipelineDef, const ShaderID& shader) const
{
	SPT_PROFILER_FUNCTION();

	return lib::HashCombine(pipelineDef, shader);
}

PipelineStateID PipelinesLibrary::GetStateID(const ShaderID& shader) const
{
	SPT_PROFILER_FUNCTION();

	return lib::GetHash(shader);
}

} // spt::rdr
