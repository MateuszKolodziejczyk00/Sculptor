#include "CommandsRecorder.h"
#include "RenderingDefinition.h"
#include "RendererBuilder.h"
#include "Types/CommandBuffer.h"
#include "Types/Barrier.h"
#include "Types/UIBackend.h"

namespace spt::renderer
{

CommandsRecorder::CommandsRecorder(const CommandsRecordingInfo& recordingInfo)
	: m_state(ECommandsRecorderState::Invalid)
{
	m_commandsBuffer = RendererBuilder::CreateCommandBuffer(recordingInfo.m_commandsBufferName, recordingInfo.m_commandBufferDef);
}

CommandsRecorder::~CommandsRecorder()
{
	SPT_CHECK(m_state == ECommandsRecorderState::Pending);
}

Bool CommandsRecorder::IsRecording() const
{
	return m_state == ECommandsRecorderState::Recording;
}

void CommandsRecorder::StartRecording(const rhi::CommandBufferUsageDefinition& commandBufferUsage)
{
	m_commandsBuffer->StartRecording(commandBufferUsage);
	m_state = ECommandsRecorderState::Recording;
}

void CommandsRecorder::FinishRecording()
{
	m_commandsBuffer->FinishRecording();
	m_state = ECommandsRecorderState::Pending;
}

const lib::SharedPtr<CommandBuffer>& CommandsRecorder::GetCommandsBuffer() const
{
	return m_commandsBuffer;
}

void CommandsRecorder::ExecuteBarrier(Barrier& barrier)
{
	SPT_PROFILE_FUNCTION();

	SPT_CHECK(IsRecording());

	barrier.GetRHI().Execute(m_commandsBuffer->GetRHI());
}

void CommandsRecorder::BeginRendering(const RenderingDefinition& definition)
{
	SPT_PROFILE_FUNCTION();

	SPT_CHECK(IsRecording());

	m_commandsBuffer->GetRHI().BeginRendering(definition.GetRHI());
}

void CommandsRecorder::EndRendering()
{
	SPT_PROFILE_FUNCTION();

	SPT_CHECK(IsRecording());

	m_commandsBuffer->GetRHI().EndRendering();
}

void CommandsRecorder::InitializeUIFonts(const lib::SharedPtr<renderer::UIBackend>& uiBackend)
{
	SPT_PROFILE_FUNCTION();

	SPT_CHECK(IsRecording());
	SPT_CHECK(!!uiBackend);

	uiBackend->GetRHI().InitializeFonts(m_commandsBuffer->GetRHI());
}

void CommandsRecorder::RenderUI(const lib::SharedPtr<renderer::UIBackend>& uiBackend)
{
	SPT_PROFILE_FUNCTION();

	SPT_CHECK(IsRecording());
	SPT_CHECK(!!uiBackend);

	uiBackend->GetRHI().Render(m_commandsBuffer->GetRHI());
}

}
