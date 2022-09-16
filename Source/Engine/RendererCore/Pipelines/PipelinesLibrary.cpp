#include "PipelinesLibrary.h"
#include "Renderer.h"
#include "Shaders/ShadersManager.h"
#include "RendererBuilder.h"

namespace spt::rdr
{

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

PipelineStateID PipelinesLibrary::GetOrCreateGfxPipeline(const RendererResourceName& nameInNotCached, const rhi::GraphicsPipelineDefinition& pipelineDef, const ShaderID& shader)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(shader.IsValid());

	const PipelineStateID stateID = GetStateID(pipelineDef, shader);

	if (!m_cachedGraphicsPipelines.contains(stateID))
	{
		const lib::LockGuard lockGuard(m_graphicsPipelinesPendingFlushLock);

		lib::SharedPtr<GraphicsPipeline>& pendingPipeline = m_graphicsPipelinesPendingFlush[stateID];

		if (!pendingPipeline)
		{
			lib::SharedRef<Shader> shaderObject = Renderer::GetShadersManager().GetShader(shader);
			pendingPipeline = RendererBuilder::CreateGraphicsPipeline(nameInNotCached, shaderObject, pipelineDef).ToSharedPtr();
		}
	}

	return stateID;
}

PipelineStateID PipelinesLibrary::GetOrCreateComputePipeline(const RendererResourceName& nameInNotCached, const ShaderID& shader)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(shader.IsValid());

	const PipelineStateID stateID = GetStateID(shader);

	if (!m_cachedComputePipelines.contains(stateID))
	{
		const lib::LockGuard lockGuard(m_computePipelinesPendingFlushLock);

		lib::SharedPtr<ComputePipeline>& pendingPipeline = m_computePipelinesPendingFlush[stateID];

		if (!pendingPipeline)
		{
			lib::SharedRef<Shader> shaderObject = Renderer::GetShadersManager().GetShader(shader);
			pendingPipeline = RendererBuilder::CreateComputePipeline(nameInNotCached, shaderObject).ToSharedPtr();
		}
	}

	return stateID;
}

lib::SharedPtr<GraphicsPipeline> PipelinesLibrary::GetGraphicsPipeline(PipelineStateID id) const
{
	return GetPipelineImpl<GraphicsPipeline>(id, m_cachedGraphicsPipelines, m_graphicsPipelinesPendingFlush, m_graphicsPipelinesPendingFlushLock);
}

lib::SharedPtr<ComputePipeline> PipelinesLibrary::GetComputePipeline(PipelineStateID id) const
{
	return GetPipelineImpl<ComputePipeline>(id, m_cachedComputePipelines, m_computePipelinesPendingFlush, m_computePipelinesPendingFlushLock);
}

void PipelinesLibrary::FlushCreatedPipelines()
{
	SPT_PROFILER_FUNCTION();

	FlushPipelinesImpl<GraphicsPipeline>(m_cachedGraphicsPipelines, m_graphicsPipelinesPendingFlush, m_graphicsPipelinesPendingFlushLock);
	FlushPipelinesImpl<ComputePipeline>(m_cachedComputePipelines, m_computePipelinesPendingFlush, m_computePipelinesPendingFlushLock);
}

void PipelinesLibrary::ClearCachedPipelines()
{
	SPT_PROFILER_FUNCTION();

	m_cachedGraphicsPipelines.clear();
	m_cachedComputePipelines.clear();

	{
		const lib::LockGuard lockGuard(m_graphicsPipelinesPendingFlushLock);
		m_graphicsPipelinesPendingFlush.clear();
	}

	{
		const lib::LockGuard lockGuard(m_computePipelinesPendingFlushLock);
		m_computePipelinesPendingFlush.clear();
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
