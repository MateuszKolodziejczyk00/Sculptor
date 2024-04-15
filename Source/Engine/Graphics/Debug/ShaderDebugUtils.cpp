#include "ShaderDebugUtils.h"
#include "RenderGraphBuilder.h"
#include "ShaderMessageCommandsExecutor.h"
#include "ShaderDebugCommandsProcessor.h"
#include "JobSystem.h"
#include "EngineFrame.h"

namespace spt::gfx::dbg
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// ShaderDebugCommandBuffer ======================================================================

ShaderDebugUtils& ShaderDebugUtils::Get()
{
	static ShaderDebugUtils instance;
	return instance;
}

void ShaderDebugUtils::Bind(rg::RenderGraphBuilder& graphBuilder, const ShaderDebugParameters& debugParameters)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(m_ds.IsValid());

	SetDebugParameters(debugParameters);

	PrepareBuffers(graphBuilder);

	PrepareDebugOutputTexture(graphBuilder, debugParameters);

	graphBuilder.BindDescriptorSetState(m_ds);
}

void ShaderDebugUtils::Unbind(rg::RenderGraphBuilder& graphBuilder)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(m_ds.IsValid());

	graphBuilder.UnbindDescriptorSetState(m_ds);

	const lib::SharedRef<rdr::Buffer> debugCommandsBuffer       = ExtractData(graphBuilder, lib::Ref(m_debugCommandsBuffer));
	const lib::SharedRef<rdr::Buffer> debugCommandsBufferOffset = ExtractData(graphBuilder, lib::Ref(m_debugCommandsBufferOffset));

	ScheduleCommandsExecution(graphBuilder, debugCommandsBuffer, debugCommandsBufferOffset);
}

void ShaderDebugUtils::BindCommandsExecutor(lib::SharedPtr<ShaderDebugCommandsExecutor> commandsExecutor)
{
	const lib::LockGuard lockGuard(m_commandsExecutorsLock);

	m_commandsExecutors.emplace_back(std::move(commandsExecutor));
}

ShaderDebugUtils::ShaderDebugUtils()
	: m_ds(rdr::ResourcesManager::CreateDescriptorSetState<ShaderDebugCommandBufferDS>(RENDERER_RESOURCE_NAME("ShaderDebugCommandBufferDS")))
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

void ShaderDebugUtils::InitializeDefaultExecutors()
{
	BindCommandsExecutor(lib::MakeShared<ShaderMessageCommandsExecutor>());
}

void ShaderDebugUtils::CleanupResources()
{
	{
		const lib::LockGuard lockGuard(m_commandsExecutorsLock);
		m_commandsExecutors.clear();
	}

	m_debugCommandsBuffer.reset();
	m_debugCommandsBufferOffset.reset();

	m_debugOutputTexture.reset();

	m_ds.Reset();
}

void ShaderDebugUtils::SetDebugParameters(const ShaderDebugParameters& debugParameters)
{
	SPT_CHECK(m_ds.IsValid());

	ShaderDebugCommandBufferParams gpuParams;
	gpuParams.mousePosition        = debugParameters.mousePosition;
	gpuParams.mousePositionHalfRes = math::Vector2i(debugParameters.mousePosition.x() >> 1, debugParameters.mousePosition.y() >> 1);
	gpuParams.bufferSize           = static_cast<Uint32>(m_debugCommandsBuffer->GetSize());

	m_ds->u_debugCommandsBufferParams = gpuParams;
}

void ShaderDebugUtils::PrepareBuffers(rg::RenderGraphBuilder& graphBuilder)
{
	SPT_PROFILER_FUNCTION();

	const rg::RGBufferViewHandle debugCommandsBufferView = graphBuilder.AcquireExternalBufferView(m_debugCommandsBufferOffset->CreateFullView());

	graphBuilder.FillBuffer(RG_DEBUG_NAME("Zero Debug Commands Buffer Offset"), debugCommandsBufferView, 0, debugCommandsBufferView->GetSize(), 0);
}

void ShaderDebugUtils::PrepareDebugOutputTexture(rg::RenderGraphBuilder& graphBuilder, const ShaderDebugParameters& debugParameters)
{
	SPT_PROFILER_FUNCTION();

	const math::Vector2u currentDebugTextureRes = m_debugOutputTexture ? m_debugOutputTexture->GetResolution2D() : math::Vector2u(0, 0);
	if (currentDebugTextureRes.x() < debugParameters.viewportSize.x() || currentDebugTextureRes.y() < debugParameters.viewportSize.y())
	{
		rhi::TextureDefinition debugOutputTextureDefinition;
		debugOutputTextureDefinition.resolution = debugParameters.viewportSize;
		debugOutputTextureDefinition.format     = rhi::EFragmentFormat::RGBA32_S_Float;
		debugOutputTextureDefinition.usage      = lib::Flags(rhi::ETextureUsage::StorageTexture, rhi::ETextureUsage::TransferSource, rhi::ETextureUsage::TransferDest);
		m_debugOutputTexture = rdr::ResourcesManager::CreateTextureView(RENDERER_RESOURCE_NAME("Shader Debug Output Texture"), debugOutputTextureDefinition, rhi::EMemoryUsage::GPUOnly);

		m_ds->u_debugOutputTexture = m_debugOutputTexture;
	}

	const rg::RGTextureViewHandle debugOutputTextureView = graphBuilder.AcquireExternalTextureView(m_debugOutputTexture);
	graphBuilder.ClearTexture(RG_DEBUG_NAME("Clear Debug Output Texture"), debugOutputTextureView, rhi::ClearColor(0.f, 0.f, 0.f, 0.f));
}

lib::SharedRef<rdr::Buffer> ShaderDebugUtils::ExtractData(rg::RenderGraphBuilder& graphBuilder, const lib::SharedRef<rdr::Buffer>& sourceBuffer)
{
	SPT_PROFILER_FUNCTION();

	const rg::RGBufferViewHandle sourceBufferView = graphBuilder.AcquireExternalBufferView(sourceBuffer->CreateFullView());

	return graphBuilder.DownloadBuffer(RG_DEBUG_NAME("ShaderDebugCommandBuffer::ExtractData"), sourceBufferView, 0, sourceBufferView->GetSize());
}

void ShaderDebugUtils::ScheduleCommandsExecution(rg::RenderGraphBuilder& graphBuilder, const lib::SharedRef<rdr::Buffer>& commands, const lib::SharedRef<rdr::Buffer>& size) const
{
	SPT_PROFILER_FUNCTION();

	lib::UniquePtr<ShaderDebugCommandsProcessor> commandsProcessor = std::make_unique<ShaderDebugCommandsProcessor>(commands, size);

	{
		const lib::LockGuard lockGuard(m_commandsExecutorsLock);

		commandsProcessor->BindExecutors(m_commandsExecutors);
	}

	js::Launch(SPT_GENERIC_JOB_NAME,
			   [jobProcessor = std::move(commandsProcessor)]
			   {
				   jobProcessor->ProcessCommands();
			   },
			   js::Prerequisites(graphBuilder.GetGPUFinishedEvent()));
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// ShaderDebugCommandsCollectingScope ============================================================

ShaderDebugScope::ShaderDebugScope(rg::RenderGraphBuilder& graphBuilder, const ShaderDebugParameters& debugParameters)
	: m_graphBuilder(graphBuilder)
{
	SPT_PROFILER_FUNCTION();

	ShaderDebugUtils::Get().Bind(graphBuilder, debugParameters);
}

ShaderDebugScope::~ShaderDebugScope()
{
	SPT_PROFILER_FUNCTION();

	ShaderDebugUtils::Get().Unbind(m_graphBuilder);
}

} // spt::gfx::dbg
