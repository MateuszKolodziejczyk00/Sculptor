#include "CommandsRecorder.h"
#include "Types/CommandBuffer.h"
#include "RendererBuilder.h"

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

}
