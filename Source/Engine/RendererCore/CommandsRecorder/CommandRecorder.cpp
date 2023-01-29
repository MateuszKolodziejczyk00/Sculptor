#include "CommandRecorder.h"
#include "RenderingDefinition.h"
#include "ResourcesManager.h"
#include "Renderer.h"
#include "Pipelines/PipelinesLibrary.h"
#include "Types/CommandBuffer.h"
#include "Types/UIBackend.h"
#include "Types/Pipeline/GraphicsPipeline.h"
#include "Types/Pipeline/ComputePipeline.h"
#include "Types/Event.h"
#include "Types/Buffer.h"

namespace spt::rdr
{

CommandRecorder::CommandRecorder()
	: m_state(ECommandsRecorderState::BuildingCommands)
{ }

CommandRecorder::~CommandRecorder()
{
	SPT_CHECK(m_state == ECommandsRecorderState::Pending);
}

Bool CommandRecorder::IsBuildingCommands() const
{
	return m_state == ECommandsRecorderState::BuildingCommands;
}

Bool CommandRecorder::IsRecording() const
{
	return m_state == ECommandsRecorderState::Recording;
}

Bool CommandRecorder::IsPending() const
{
	return m_state == ECommandsRecorderState::Pending;
}

void CommandRecorder::RecordCommands(const lib::SharedRef<RenderContext>& context, const CommandsRecordingInfo& recordingInfo, const rhi::CommandBufferUsageDefinition& commandBufferUsage)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(IsBuildingCommands());

	m_state = ECommandsRecorderState::Recording;

	m_pipelineState.PrepareForExecution(context);
	
	m_commandsBuffer = ResourcesManager::CreateCommandBuffer(recordingInfo.commandsBufferName, context, recordingInfo.commandBufferDef);
	m_commandsBuffer->StartRecording(commandBufferUsage);

	CommandQueueExecutor executor(lib::Ref(m_commandsBuffer), context);
	m_commandQueue.ExecuteAndReset(executor);

	m_commandsBuffer->FinishRecording();

	m_state = ECommandsRecorderState::Pending;
}

const lib::SharedPtr<CommandBuffer>& CommandRecorder::GetCommandBuffer() const
{
	return m_commandsBuffer;
}

void CommandRecorder::ExecuteBarrier(rhi::RHIDependency dependency)
{
	SPT_PROFILER_FUNCTION();

	EnqueueRenderCommand([localDependency = std::move(dependency)](const lib::SharedRef<CommandBuffer>& cmdBuffer, const CommandExecuteContext& executionContext) mutable
						 {
							 SPT_PROFILER_SCOPE("ExecuteBarrier Command");
							
							 localDependency.ExecuteBarrier(cmdBuffer->GetRHI());
						 });
}

void CommandRecorder::SetEvent(const lib::SharedRef<Event>& event, rhi::RHIDependency dependency)
{
	SPT_PROFILER_FUNCTION();

	EnqueueRenderCommand([event, localDependency = std::move(dependency)](const lib::SharedRef<CommandBuffer>& cmdBuffer, const CommandExecuteContext& executionContext) mutable
						 {
							 SPT_PROFILER_SCOPE("SetEvent Command");
							
							 localDependency.SetEvent(cmdBuffer->GetRHI(), event->GetRHI());
						 });
}

void CommandRecorder::WaitEvent(const lib::SharedRef<Event>& event, rhi::RHIDependency dependency)
{
	SPT_PROFILER_FUNCTION();

	EnqueueRenderCommand([event, localDependency = std::move(dependency)](const lib::SharedRef<CommandBuffer>& cmdBuffer, const CommandExecuteContext& executionContext) mutable
						 {
							 SPT_PROFILER_SCOPE("WaitEvent Command");
							
							 localDependency.WaitEvent(cmdBuffer->GetRHI(), event->GetRHI());
						 });
}

void CommandRecorder::BeginRendering(const RenderingDefinition& definition)
{
	SPT_PROFILER_FUNCTION();

	EnqueueRenderCommand([definition](const lib::SharedRef<CommandBuffer>& cmdBuffer, const CommandExecuteContext& executionContext)
						 {
							 SPT_PROFILER_SCOPE("BeginRendering Command");

							 cmdBuffer->GetRHI().BeginRendering(definition.GetRHI());
						 });
}

void CommandRecorder::EndRendering()
{
	SPT_PROFILER_FUNCTION();

	EnqueueRenderCommand([](const lib::SharedRef<CommandBuffer>& cmdBuffer, const CommandExecuteContext& executionContext)
						 {
							 SPT_PROFILER_SCOPE("EndRendering Command");

							 cmdBuffer->GetRHI().EndRendering();
						 });
}

void CommandRecorder::SetViewport(const math::AlignedBox2f& renderingViewport, Real32 minDepth, Real32 maxDepth)
{
	SPT_PROFILER_FUNCTION();

	EnqueueRenderCommand([=](const lib::SharedRef<CommandBuffer>& cmdBuffer, const CommandExecuteContext& executionContext)
						 {
							 SPT_PROFILER_SCOPE("SetScissor Command");

							 cmdBuffer->GetRHI().SetViewport(renderingViewport, minDepth, maxDepth);
						 });
}

void CommandRecorder::SetScissor(const math::AlignedBox2u& renderingScissor)
{
	SPT_PROFILER_FUNCTION();

	EnqueueRenderCommand([=](const lib::SharedRef<CommandBuffer>& cmdBuffer, const CommandExecuteContext& executionContext)
						 {
							 SPT_PROFILER_SCOPE("SetScissor Command");

							 cmdBuffer->GetRHI().SetScissor(renderingScissor);
						 });
}

void CommandRecorder::DrawIndirectCount(const lib::SharedRef<Buffer>& drawsBuffer, Uint64 drawsOffset, Uint32 drawsStride, const lib::SharedRef<Buffer>& countBuffer, Uint64 countOffset, Uint32 maxDrawsCount)
{
	SPT_PROFILER_FUNCTION();

	m_pipelineState.EnqueueFlushDirtyDSForGraphicsPipeline(m_commandQueue);

	EnqueueRenderCommand([=](const lib::SharedRef<CommandBuffer>& cmdBuffer, const CommandExecuteContext& executionContext)
						 {
							 SPT_PROFILER_SCOPE("DrawIndirectCount Command");

							 cmdBuffer->GetRHI().DrawIndirectCount(drawsBuffer->GetRHI(), drawsOffset, drawsStride, countBuffer->GetRHI(), countOffset, maxDrawsCount);
						 });
}

void CommandRecorder::DrawIndirectCount(const BufferView& drawsBufferView, Uint64 drawsOffset, Uint32 drawsStride, const BufferView& countBufferView, Uint64 countOffset, Uint32 maxDrawsCount)
{
	SPT_PROFILER_FUNCTION();

	DrawIndirectCount(drawsBufferView.GetBuffer(), drawsBufferView.GetOffset() + drawsOffset, drawsStride, countBufferView.GetBuffer(), countBufferView.GetOffset() + countOffset, maxDrawsCount);
}

void CommandRecorder::DrawIndirect(const lib::SharedRef<Buffer>& drawsBuffer, Uint64 drawsOffset, Uint32 drawsStride, Uint32 drawsCount)
{
	SPT_PROFILER_FUNCTION();

	m_pipelineState.EnqueueFlushDirtyDSForGraphicsPipeline(m_commandQueue);

	EnqueueRenderCommand([=](const lib::SharedRef<CommandBuffer>& cmdBuffer, const CommandExecuteContext& executionContext)
						 {
							 SPT_PROFILER_SCOPE("DrawIndirect Command");

							 cmdBuffer->GetRHI().DrawIndirect(drawsBuffer->GetRHI(), drawsOffset, drawsStride, drawsCount);
						 });
}

void CommandRecorder::DrawIndirect(const BufferView& drawsBufferView, Uint64 drawsOffset, Uint32 drawsStride, Uint32 drawsCount)
{
	SPT_PROFILER_FUNCTION();

	DrawIndirect(drawsBufferView.GetBuffer(), drawsBufferView.GetOffset() + drawsOffset, drawsStride, drawsCount);
}

void CommandRecorder::BindGraphicsPipeline(PipelineStateID pipelineID)
{
	SPT_PROFILER_FUNCTION();

	const lib::SharedPtr<GraphicsPipeline> pipeline = Renderer::GetPipelinesLibrary().GetGraphicsPipeline(pipelineID);
	SPT_CHECK(!!pipeline);

	m_pipelineState.BindGraphicsPipeline(lib::Ref(pipeline));

	EnqueueRenderCommand([pipeline](const lib::SharedRef<CommandBuffer>& cmdBuffer, const CommandExecuteContext& executionContext)
						 {
							 SPT_PROFILER_SCOPE("BindGraphicsPipeline Command");

							 cmdBuffer->GetRHI().BindGfxPipeline(pipeline->GetRHI());
						 });
}

void CommandRecorder::BindGraphicsPipeline(const rhi::GraphicsPipelineDefinition& pipelineDef, const ShaderID& shader)
{
	SPT_PROFILER_FUNCTION();

	PipelineStateID pipelineID;
	BindGraphicsPipeline(pipelineDef, shader, pipelineID);
}

void CommandRecorder::BindGraphicsPipeline(const rhi::GraphicsPipelineDefinition& pipelineDef, const ShaderID& shader, INOUT PipelineStateID& cachedPipelineID)
{
	SPT_PROFILER_FUNCTION();

	if(!cachedPipelineID.IsValid())
	{
		cachedPipelineID = Renderer::GetPipelinesLibrary().GetOrCreateGfxPipeline(RENDERER_RESOURCE_NAME(shader.GetName()), pipelineDef, shader);
	}
	SPT_CHECK(cachedPipelineID.IsValid());

	BindGraphicsPipeline(cachedPipelineID);
}

void CommandRecorder::BindComputePipeline(PipelineStateID pipelineID)
{
	SPT_PROFILER_FUNCTION();

	const lib::SharedPtr<ComputePipeline> pipeline = Renderer::GetPipelinesLibrary().GetComputePipeline(pipelineID);
	SPT_CHECK(!!pipeline);

	m_pipelineState.BindComputePipeline(lib::Ref(pipeline));

	EnqueueRenderCommand([pipeline](const lib::SharedRef<CommandBuffer>& cmdBuffer, const CommandExecuteContext& executionContext)
						 {
							 SPT_PROFILER_SCOPE("BindComputePipeline Command");

							 cmdBuffer->GetRHI().BindComputePipeline(pipeline->GetRHI());
						 });
}

void CommandRecorder::BindComputePipeline(const ShaderID& shader)
{
	SPT_PROFILER_FUNCTION();

	const PipelineStateID pipelineID = Renderer::GetPipelinesLibrary().GetOrCreateComputePipeline(RENDERER_RESOURCE_NAME(shader.GetName()), shader);
	BindComputePipeline(pipelineID);
}

void CommandRecorder::Dispatch(const math::Vector3u& groupCount)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(!!m_pipelineState.GetBoundComputePipeline());

	m_pipelineState.EnqueueFlushDirtyDSForComputePipeline(m_commandQueue);

	EnqueueRenderCommand([groupCount](const lib::SharedRef<CommandBuffer>& cmdBuffer, const CommandExecuteContext& executionContext)
						 {
							 SPT_PROFILER_SCOPE("Dispatch Command");

							 cmdBuffer->GetRHI().Dispatch(groupCount);
						 });
}

void CommandRecorder::DispatchIndirect(const lib::SharedRef<Buffer>& indirectArgsBuffer, Uint64 indirectArgsOffset)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(!!m_pipelineState.GetBoundComputePipeline());

	m_pipelineState.EnqueueFlushDirtyDSForComputePipeline(m_commandQueue);

	EnqueueRenderCommand([=](const lib::SharedRef<CommandBuffer>& cmdBuffer, const CommandExecuteContext& executionContext)
						 {
							 SPT_PROFILER_SCOPE("DispatchIndirect Command");

							 cmdBuffer->GetRHI().DispatchIndirect(indirectArgsBuffer->GetRHI(), indirectArgsOffset);
						 });
}

void CommandRecorder::DispatchIndirect(const BufferView& indirectArgsBufferView, Uint64 indirectArgsOffset)
{
	DispatchIndirect(indirectArgsBufferView.GetBuffer(), indirectArgsBufferView.GetOffset() + indirectArgsOffset);
}

void CommandRecorder::BindDescriptorSetState(const lib::SharedRef<DescriptorSetState>& state)
{
	SPT_PROFILER_FUNCTION();

	m_pipelineState.BindDescriptorSetState(state);
}

void CommandRecorder::UnbindDescriptorSetState(const lib::SharedRef<DescriptorSetState>& state)
{
	SPT_PROFILER_FUNCTION();

	m_pipelineState.UnbindDescriptorSetState(state);
}

void CommandRecorder::BlitTexture(const lib::SharedRef<Texture>& source, Uint32 sourceMipLevel, Uint32 sourceArrayLayer, const lib::SharedRef<Texture>& dest, Uint32 destMipLevel, Uint32 destArrayLayer, rhi::ETextureAspect aspect, rhi::ESamplerFilterType filterMode)
{
	SPT_PROFILER_FUNCTION();

	EnqueueRenderCommand([=](const lib::SharedRef<CommandBuffer>& cmdBuffer, const CommandExecuteContext& executionContext)
						 {
							 SPT_PROFILER_SCOPE("BlitTexture Command");

							 cmdBuffer->GetRHI().BlitTexture(source->GetRHI(), sourceMipLevel, sourceArrayLayer, dest->GetRHI(), destMipLevel, destArrayLayer, aspect, filterMode);
						 });
}

void CommandRecorder::CopyTexture(const lib::SharedRef<Texture>& source, const rhi::TextureCopyRange& sourceRange, const lib::SharedRef<Texture>& target, const rhi::TextureCopyRange& targetRange, const math::Vector3u& extent)
{
	SPT_PROFILER_FUNCTION();

	EnqueueRenderCommand([source, sourceRange, target, targetRange, extent](const lib::SharedRef<CommandBuffer>& cmdBuffer, const CommandExecuteContext& executionContext)
						 {
							 SPT_PROFILER_SCOPE("CopyTexture Command");

							 cmdBuffer->GetRHI().CopyTexture(source->GetRHI(), sourceRange, target->GetRHI(), targetRange, extent);
						 });
}

void CommandRecorder::CopyBuffer(const lib::SharedRef<Buffer>& sourceBuffer, Uint64 sourceOffset, const lib::SharedRef<Buffer>& destBuffer, Uint64 destOffset, Uint64 size)
{
	SPT_PROFILER_FUNCTION();

	EnqueueRenderCommand([sourceBuffer, sourceOffset, destBuffer, destOffset, size](const lib::SharedRef<CommandBuffer>& cmdBuffer, const CommandExecuteContext& executionContext)
						 {
							 SPT_PROFILER_SCOPE("CopyBuffer Command");

							 cmdBuffer->GetRHI().CopyBuffer(sourceBuffer->GetRHI(), sourceOffset, destBuffer->GetRHI(), destOffset, size);
						 });
}

void CommandRecorder::FillBuffer(const lib::SharedRef<Buffer>& buffer, Uint64 offset, Uint64 range, Uint32 data)
{
	SPT_PROFILER_FUNCTION();

	EnqueueRenderCommand([buffer, offset, range, data](const lib::SharedRef<CommandBuffer>& cmdBuffer, const CommandExecuteContext& executionContext)
						 {
							 SPT_PROFILER_SCOPE("FillBuffer Command");

							 cmdBuffer->GetRHI().FillBuffer(buffer->GetRHI(), offset, range, data);
						 });
}

void CommandRecorder::CopyBufferToTexture(const lib::SharedRef<Buffer>& buffer, Uint64 bufferOffset, const lib::SharedRef<Texture>& texture, rhi::ETextureAspect aspect, math::Vector3u copyExtent, math::Vector3u copyOffset /*= math::Vector3u::Zero()*/, Uint32 mipLevel /*= 0*/, Uint32 arrayLayer /*= 0*/)
{
	SPT_PROFILER_FUNCTION();

	EnqueueRenderCommand([=](const lib::SharedRef<CommandBuffer>& cmdBuffer, const CommandExecuteContext& executionContext)
						 {
							 SPT_PROFILER_SCOPE("CopyBufferToTexture Command");

							 cmdBuffer->GetRHI().CopyBufferToTexture(buffer->GetRHI(), bufferOffset, texture->GetRHI(), aspect, copyExtent, copyOffset, mipLevel, arrayLayer);
						 });
}

#if WITH_GPU_CRASH_DUMPS
void CommandRecorder::SetDebugCheckpoint(const lib::HashedString& marker)
{
	SPT_PROFILER_FUNCTION();

	EnqueueRenderCommand([marker](const lib::SharedRef<CommandBuffer>& cmdBuffer, const CommandExecuteContext& executionContext)
						 {
							 SPT_PROFILER_SCOPE("SetDebugCheckpoint Command");

							 cmdBuffer->GetRHI().SetDebugCheckpoint(reinterpret_cast<const void*>(marker.GetKey()));
						 });
}
#endif // WITH_GPU_CRASH_DUMPS

void CommandRecorder::InitializeUIFonts()
{
	SPT_PROFILER_FUNCTION();

	EnqueueRenderCommand([](const lib::SharedRef<CommandBuffer>& cmdBuffer, const CommandExecuteContext& executionContext)
						 {
							 SPT_PROFILER_SCOPE("InitializeUIFonts Command");

							 UIBackend::GetRHI().InitializeFonts(cmdBuffer->GetRHI());
						 });
}

void CommandRecorder::RenderUI()
{
	SPT_PROFILER_FUNCTION();

	EnqueueRenderCommand([](const lib::SharedRef<CommandBuffer>& cmdBuffer, const CommandExecuteContext& executionContext)
						 {
							 SPT_PROFILER_SCOPE("RenderUI Command");

							 UIBackend::GetRHI().Render(cmdBuffer->GetRHI());
						 });
}

void CommandRecorder::BeginDebugRegion(const lib::HashedString& name, const lib::Color& color)
{
	SPT_PROFILER_FUNCTION();

	EnqueueRenderCommand([name, color](const lib::SharedRef<CommandBuffer>& cmdBuffer, const CommandExecuteContext& executionContext)
						 {
							 SPT_PROFILER_SCOPE("BeginDebugRegion Command");

							 cmdBuffer->GetRHI().BeginDebugRegion(name, color);
						 });
}

void CommandRecorder::EndDebugRegion()
{
	SPT_PROFILER_FUNCTION();

	EnqueueRenderCommand([](const lib::SharedRef<CommandBuffer>& cmdBuffer, const CommandExecuteContext& executionContext)
						 {
							 SPT_PROFILER_SCOPE("EndDebugRegion Command");

							 cmdBuffer->GetRHI().EndDebugRegion();
						 });
}

} // spt::rdr