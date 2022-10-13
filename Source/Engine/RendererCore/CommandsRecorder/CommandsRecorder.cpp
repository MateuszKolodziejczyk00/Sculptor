#include "CommandsRecorder.h"
#include "RenderingDefinition.h"
#include "ResourcesManager.h"
#include "Renderer.h"
#include "Pipelines/PipelinesLibrary.h"
#include "Types/CommandBuffer.h"
#include "Types/Barrier.h"
#include "Types/UIBackend.h"
#include "Types/Pipeline/GraphicsPipeline.h"
#include "Types/Pipeline/ComputePipeline.h"

namespace spt::rdr
{

CommandsRecorder::CommandsRecorder()
	: m_state(ECommandsRecorderState::BuildingCommands)
{ }

CommandsRecorder::~CommandsRecorder()
{
	SPT_CHECK(m_state == ECommandsRecorderState::Pending);
}

Bool CommandsRecorder::IsBuildingCommands() const
{
	return m_state == ECommandsRecorderState::BuildingCommands;
}

Bool CommandsRecorder::IsRecording() const
{
	return m_state == ECommandsRecorderState::Recording;
}

Bool CommandsRecorder::IsPending() const
{
	return m_state == ECommandsRecorderState::Pending;
}

void CommandsRecorder::RecordCommands(const lib::SharedRef<Context>& context, const CommandsRecordingInfo& recordingInfo, const rhi::CommandBufferUsageDefinition& commandBufferUsage)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(IsBuildingCommands());

	m_state = ECommandsRecorderState::Recording;
	
	m_commandsBuffer = ResourcesManager::CreateCommandBuffer(recordingInfo.commandsBufferName, recordingInfo.commandBufferDef);
	m_commandsBuffer->StartRecording(commandBufferUsage);

	CommandQueueExecutor executor(lib::Ref(m_commandsBuffer));
	m_commandQueue.ExecuteAndReset(executor);

	m_commandsBuffer->FinishRecording();

	m_state = ECommandsRecorderState::Pending;
}

const lib::SharedPtr<CommandBuffer>& CommandsRecorder::GetCommandBuffer() const
{
	return m_commandsBuffer;
}

void CommandsRecorder::ExecuteBarrier(Barrier barrier)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(IsBuildingCommands());

	EnqueueRenderCommand([localBarrier = std::move(barrier)](const lib::SharedRef<CommandBuffer>& cmdBuffer, const CommandExecuteContext& executionContext) mutable
						 {
							 localBarrier.GetRHI().Execute(cmdBuffer->GetRHI());
						 });
}

void CommandsRecorder::BeginRendering(const RenderingDefinition& definition)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(IsBuildingCommands());

	EnqueueRenderCommand([definition](const lib::SharedRef<CommandBuffer>& cmdBuffer, const CommandExecuteContext& executionContext)
						 {
							 cmdBuffer->GetRHI().BeginRendering(definition.GetRHI());
						 });
}

void CommandsRecorder::EndRendering()
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(IsBuildingCommands());

	EnqueueRenderCommand([](const lib::SharedRef<CommandBuffer>& cmdBuffer, const CommandExecuteContext& executionContext)
						 {
							 cmdBuffer->GetRHI().EndRendering();
						 });
}

void CommandsRecorder::BindGraphicsPipeline(PipelineStateID pipelineID)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(IsBuildingCommands());

	const lib::SharedPtr<GraphicsPipeline> pipeline = Renderer::GetPipelinesLibrary().GetGraphicsPipeline(pipelineID);
	SPT_CHECK(!!pipeline);

	m_pipelineState.BindGraphicsPipeline(lib::Ref(pipeline));

	EnqueueRenderCommand([pipeline](const lib::SharedRef<CommandBuffer>& cmdBuffer, const CommandExecuteContext& executionContext)
						 {
							 cmdBuffer->GetRHI().BindGfxPipeline(pipeline->GetRHI());
						 });
}

void CommandsRecorder::BindGraphicsPipeline(const rhi::GraphicsPipelineDefinition& pipelineDef, const ShaderID& shader)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(IsBuildingCommands());

	const PipelineStateID pipelineID = Renderer::GetPipelinesLibrary().GetOrCreateGfxPipeline(RENDERER_RESOURCE_NAME(shader.GetName()), pipelineDef, shader);
	BindGraphicsPipeline(pipelineID);
}

void CommandsRecorder::BindComputePipeline(PipelineStateID pipelineID)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(IsBuildingCommands());

	const lib::SharedPtr<ComputePipeline> pipeline = Renderer::GetPipelinesLibrary().GetComputePipeline(pipelineID);
	SPT_CHECK(!!pipeline);

	m_pipelineState.BindComputePipeline(lib::Ref(pipeline));

	EnqueueRenderCommand([pipeline](const lib::SharedRef<CommandBuffer>& cmdBuffer, const CommandExecuteContext& executionContext)
						 {
							 cmdBuffer->GetRHI().BindComputePipeline(pipeline->GetRHI());
						 });
}

void CommandsRecorder::BindComputePipeline(const ShaderID& shader)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(IsBuildingCommands());

	const PipelineStateID pipelineID = Renderer::GetPipelinesLibrary().GetOrCreateComputePipeline(RENDERER_RESOURCE_NAME(shader.GetName()), shader);
	BindComputePipeline(pipelineID);
}

void CommandsRecorder::Dispatch(const math::Vector3u& groupCount)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(IsBuildingCommands());
	SPT_CHECK(!!m_pipelineState.GetBoundComputePipeline());

	m_pipelineState.EnqueueFlushDirtyDSForComputePipeline(m_commandQueue);

	EnqueueRenderCommand([groupCount](const lib::SharedRef<CommandBuffer>& cmdBuffer, const CommandExecuteContext& executionContext)
						 {
							 cmdBuffer->GetRHI().Dispatch(groupCount);
						 });
}


void CommandsRecorder::BindDescriptorSetState(const lib::SharedRef<DescriptorSetState>& state)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(IsBuildingCommands());

	m_pipelineState.BindDescriptorSetState(state);
}

void CommandsRecorder::UnbindDescriptorSetState(const lib::SharedRef<DescriptorSetState>& state)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(IsBuildingCommands());

	m_pipelineState.UnbindDescriptorSetState(state);
}

void CommandsRecorder::CopyTexture(const lib::SharedRef<Texture>& source, const rhi::TextureCopyRange& sourceRange, const lib::SharedRef<Texture>& target, const rhi::TextureCopyRange& targetRange, const math::Vector3u& extent)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(IsBuildingCommands());

	EnqueueRenderCommand([source, sourceRange, target, targetRange, extent](const lib::SharedRef<CommandBuffer>& cmdBuffer, const CommandExecuteContext& executionContext)
						 {
							 cmdBuffer->GetRHI().CopyTexture(source->GetRHI(), sourceRange, target->GetRHI(), targetRange, extent);
						 });
}

void CommandsRecorder::InitializeUIFonts()
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(IsBuildingCommands());

	EnqueueRenderCommand([](const lib::SharedRef<CommandBuffer>& cmdBuffer, const CommandExecuteContext& executionContext)
						 {
							 UIBackend::GetRHI().InitializeFonts(cmdBuffer->GetRHI());
						 });
}

void CommandsRecorder::RenderUI()
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(IsBuildingCommands());

	EnqueueRenderCommand([](const lib::SharedRef<CommandBuffer>& cmdBuffer, const CommandExecuteContext& executionContext)
						 {
							 UIBackend::GetRHI().Render(cmdBuffer->GetRHI());
						 });
}

void CommandsRecorder::BeginDebugRegion(const lib::HashedString& name, const lib::Color& color)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(IsBuildingCommands());

	EnqueueRenderCommand([name, color](const lib::SharedRef<CommandBuffer>& cmdBuffer, const CommandExecuteContext& executionContext)
						 {
							 cmdBuffer->GetRHI().BeginDebugRegion(name, color);
						 });
}

void CommandsRecorder::EndDebugRegion()
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(IsBuildingCommands());

	EnqueueRenderCommand([](const lib::SharedRef<CommandBuffer>& cmdBuffer, const CommandExecuteContext& executionContext)
						 {
							 cmdBuffer->GetRHI().EndDebugRegion();
						 });
}

} // spt::rdr
