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

	const PipelineStateID pipelineID = Renderer::GetPipelinesLibrary().GetOrCreateGfxPipeline(RENDERER_RESOURCE_NAME(shader.GetName()), pipelineDef, shader);
	BindGraphicsPipeline(pipelineID);
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

void CommandRecorder::CopyTexture(const lib::SharedRef<Texture>& source, const rhi::TextureCopyRange& sourceRange, const lib::SharedRef<Texture>& target, const rhi::TextureCopyRange& targetRange, const math::Vector3u& extent)
{
	SPT_PROFILER_FUNCTION();

	EnqueueRenderCommand([source, sourceRange, target, targetRange, extent](const lib::SharedRef<CommandBuffer>& cmdBuffer, const CommandExecuteContext& executionContext)
						 {
							 SPT_PROFILER_SCOPE("CopyTexture Command");

							 cmdBuffer->GetRHI().CopyTexture(source->GetRHI(), sourceRange, target->GetRHI(), targetRange, extent);
						 });
}

#if WITH_GPU_CRASH_DUMPS
void CommandRecorder::SetDebugCheckpoint(const void* markerPtr)
{
	SPT_PROFILER_FUNCTION();

	EnqueueRenderCommand([markerPtr](const lib::SharedRef<CommandBuffer>& cmdBuffer, const CommandExecuteContext& executionContext)
						 {
							 SPT_PROFILER_SCOPE("SetDebugCheckpoint Command");

							 cmdBuffer->GetRHI().SetDebugCheckpoint(markerPtr);
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