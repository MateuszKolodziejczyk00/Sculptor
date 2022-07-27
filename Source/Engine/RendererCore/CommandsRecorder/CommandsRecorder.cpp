#include "CommandsRecorder.h"
#include "Types/CommandBuffer.h"
#include "RendererBuilder.h"

namespace spt::renderer
{

CommandsRecorder::CommandsRecorder(const CommandsRecordingInfo& recordingInfo)
{
	m_commandsBuffer = RendererBuilder::CreateCommandBuffer(RENDERER_RESOURCE_NAME(recordingInfo.m_commandsBufferName), recordingInfo.m_commandBufferDef);
}

CommandsRecorder::~CommandsRecorder()
{
	SPT_CHECK(!IsRecording());
}

Bool CommandsRecorder::IsRecording() const
{
	return !!m_commandsBuffer;
}

void CommandsRecorder::FinishRecording()
{
	m_commandsBuffer.reset();
}

}
