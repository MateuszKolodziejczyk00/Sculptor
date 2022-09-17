#include "CommandsRecorder.h"
#include "RenderingDefinition.h"
#include "RendererBuilder.h"
#include "Types/CommandBuffer.h"
#include "Types/Barrier.h"
#include "Types/UIBackend.h"

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
	
	m_commandsBuffer = RendererBuilder::CreateCommandBuffer(recordingInfo.commandsBufferName, recordingInfo.commandBufferDef);
	m_commandsBuffer->StartRecording(commandBufferUsage);

	CommandQueueExecutor executor(m_commandsBuffer);
	m_commandQueue.ExecuteAndReset(executor);

	m_commandsBuffer->FinishRecording();

	m_state = ECommandsRecorderState::Pending;
}

const lib::SharedPtr<CommandBuffer>& CommandsRecorder::GetCommandsBuffer() const
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

void CommandsRecorder::InitializeUIFonts(const lib::SharedRef<rdr::UIBackend>& uiBackend)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(IsBuildingCommands());

	EnqueueRenderCommand([uiBackend](const lib::SharedRef<CommandBuffer>& cmdBuffer, const CommandExecuteContext& executionContext)
						 {
							 uiBackend->GetRHI().InitializeFonts(cmdBuffer->GetRHI());
						 });
}

void CommandsRecorder::RenderUI(const lib::SharedRef<rdr::UIBackend>& uiBackend)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(IsBuildingCommands());

	EnqueueRenderCommand([uiBackend](const lib::SharedRef<CommandBuffer>& cmdBuffer, const CommandExecuteContext& executionContext)
						 {
							 uiBackend->GetRHI().Render(cmdBuffer->GetRHI());
						 });
}

} // spt::rdr
