#include "ShaderDebugCommandBuffer.h"
#include "RenderGraphBuilder.h"
#include "ShaderPrintfExecutor.h"
#include "ShaderDebugCommandsProcessor.h"
#include "JobSystem.h"
#include "EngineFrame.h"

namespace spt::gfx::dbg
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// ShaderDebugCommandBuffer ======================================================================

ShaderDebugCommandBuffer& ShaderDebugCommandBuffer::Get()
{
	static ShaderDebugCommandBuffer instance;
	return instance;
}

void ShaderDebugCommandBuffer::Bind(rg::RenderGraphBuilder& graphBuilder, const ShaderDebugParameters& debugParameters)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(m_ds.IsValid());

	SetDebugParameters(debugParameters);

	PrepareBuffers(graphBuilder);

	graphBuilder.BindDescriptorSetState(m_ds);
}

void ShaderDebugCommandBuffer::Unbind(rg::RenderGraphBuilder& graphBuilder)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(m_ds.IsValid());

	graphBuilder.UnbindDescriptorSetState(m_ds);

	const lib::SharedRef<rdr::Buffer> debugCommandsBuffer       = ExtractData(graphBuilder, lib::Ref(m_debugCommandsBuffer));
	const lib::SharedRef<rdr::Buffer> debugCommandsBufferOffset = ExtractData(graphBuilder, lib::Ref(m_debugCommandsBufferOffset));

	ScheduleCommandsExecution(debugCommandsBuffer, debugCommandsBufferOffset);
}

void ShaderDebugCommandBuffer::BindCommandsExecutor(lib::SharedPtr<ShaderDebugCommandsExecutor> commandsExecutor)
{
	const lib::LockGuard lockGuard(m_commandsExecutorsLock);

	m_commandsExecutors.emplace_back(std::move(commandsExecutor));
}

ShaderDebugCommandBuffer::ShaderDebugCommandBuffer()
	: m_ds(rdr::ResourcesManager::CreateDescriptorSetState<ShaderDebugCommandBufferDS>(RENDERER_RESOURCE_NAME("ShaderDebugCommandBufferDS"), rdr::EDescriptorSetStateFlags::Persistent))
{
	const Uint32 bufferSize = 1024 * 1024 * 4;

	rhi::BufferDefinition debugCommandsBufferDefinition;
	debugCommandsBufferDefinition.size  = static_cast<Uint64>(bufferSize);
	debugCommandsBufferDefinition.usage = lib::Flags(rhi::EBufferUsage::Storage, rhi::EBufferUsage::TransferSrc);
	m_debugCommandsBuffer = rdr::ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME("ShaderDebugCommandBuffer"), debugCommandsBufferDefinition, rhi::EMemoryUsage::GPUOnly);

	rhi::BufferDefinition debugCommandsBufferOffsetDefinition;
	debugCommandsBufferOffsetDefinition.size  = sizeof(Uint32);
	debugCommandsBufferOffsetDefinition.usage = lib::Flags(rhi::EBufferUsage::Storage, rhi::EBufferUsage::TransferSrc, rhi::EBufferUsage::TransferDst);
	m_debugCommandsBufferOffset = rdr::ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME("ShaderDebugCommandBufferOffset"), debugCommandsBufferOffsetDefinition, rhi::EMemoryUsage::GPUOnly);

	ShaderDebugCommandBufferParams params;
	params.bufferSize = bufferSize;

	m_ds->u_debugCommandsBuffer       = m_debugCommandsBuffer->CreateFullView();
	m_ds->u_debugCommandsBufferOffset = m_debugCommandsBufferOffset->CreateFullView();
	m_ds->u_debugCommandsBufferParams = params;

	InitializeDefaultExecutors();

	rdr::Renderer::GetOnRendererCleanupDelegate().AddLambda([this]
															{
																CleanupResources();
															});
}

void ShaderDebugCommandBuffer::InitializeDefaultExecutors()
{
	BindCommandsExecutor(lib::MakeShared<ShaderPrintfExecutor>());
}

void ShaderDebugCommandBuffer::CleanupResources()
{
	{
		const lib::LockGuard lockGuard(m_commandsExecutorsLock);
		m_commandsExecutors.clear();
	}

	m_debugCommandsBuffer.reset();
	m_debugCommandsBufferOffset.reset();

	m_ds.Reset();
}

void ShaderDebugCommandBuffer::SetDebugParameters(const ShaderDebugParameters& debugParameters)
{
	SPT_CHECK(m_ds.IsValid());

	ShaderDebugCommandBufferParams gpuParams;
	gpuParams.mousePosition        = debugParameters.mousePosition;
	gpuParams.mousePositionHalfRes = math::Vector2i(debugParameters.mousePosition.x() >> 1, debugParameters.mousePosition.y() >> 1);
	gpuParams.bufferSize           = static_cast<Uint32>(m_debugCommandsBuffer->GetSize());

	m_ds->u_debugCommandsBufferParams = gpuParams;
}

void ShaderDebugCommandBuffer::PrepareBuffers(rg::RenderGraphBuilder& graphBuilder)
{
	SPT_PROFILER_FUNCTION();

	const rg::RGBufferViewHandle debugCommandsBufferView = graphBuilder.AcquireExternalBufferView(m_debugCommandsBufferOffset->CreateFullView());

	graphBuilder.FillBuffer(RG_DEBUG_NAME("Zero Debug Commands Buffer Offset"), debugCommandsBufferView, 0, debugCommandsBufferView->GetSize(), 0);
}

lib::SharedRef<rdr::Buffer> ShaderDebugCommandBuffer::ExtractData(rg::RenderGraphBuilder& graphBuilder, const lib::SharedRef<rdr::Buffer>& sourceBuffer)
{
	SPT_PROFILER_FUNCTION();

	const rg::RGBufferViewHandle sourceBufferView = graphBuilder.AcquireExternalBufferView(sourceBuffer->CreateFullView());

	return graphBuilder.DownloadBuffer(RG_DEBUG_NAME("ShaderDebugCommandBuffer::ExtractData"), sourceBufferView, 0, sourceBufferView->GetSize());
}

void ShaderDebugCommandBuffer::ScheduleCommandsExecution(const lib::SharedRef<rdr::Buffer>& commands, const lib::SharedRef<rdr::Buffer>& size) const
{
	SPT_PROFILER_FUNCTION();

	lib::UniquePtr<ShaderDebugCommandsProcessor> commandsProcessor = std::make_unique<ShaderDebugCommandsProcessor>(commands, size);

	{
		const lib::LockGuard lockGuard(m_commandsExecutorsLock);

		commandsProcessor->BindExecutors(m_commandsExecutors);
	}

	engn::OnGPUFinished::Delegate delegate;
	delegate.BindLambda([processor = std::move(commandsProcessor)](engn::FrameContext& context) mutable
					   {
						   js::Launch(SPT_GENERIC_JOB_NAME,
									  [jobProcessor = std::move(processor)]
									  {
										  jobProcessor->ProcessCommands();
									  });
					   });

	engn::GetRenderingFrame().AddOnGPUFinishedDelegate(std::move(delegate));
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// ShaderDebugCommandsCollectingScope ============================================================

ShaderDebugCommandsCollectingScope::ShaderDebugCommandsCollectingScope(rg::RenderGraphBuilder& graphBuilder, const ShaderDebugParameters& debugParameters)
	: m_graphBuilder(graphBuilder)
{
	SPT_PROFILER_FUNCTION();

	ShaderDebugCommandBuffer::Get().Bind(graphBuilder, debugParameters);
}

ShaderDebugCommandsCollectingScope::~ShaderDebugCommandsCollectingScope()
{
	SPT_PROFILER_FUNCTION();

	ShaderDebugCommandBuffer::Get().Unbind(m_graphBuilder);
}

} // spt::gfx::dbg
