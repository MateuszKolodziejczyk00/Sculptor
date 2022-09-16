#include "CommandsRecorder.h"
#include "RenderingDefinition.h"
#include "RendererBuilder.h"
#include "Types/CommandBuffer.h"
#include "Types/Barrier.h"
#include "Types/UIBackend.h"

namespace spt::rdr
{

CommandsRecorder::CommandsRecorder(const lib::SharedRef<Context>& context, const CommandsRecordingInfo& recordingInfo)
	: m_context(context)
	, m_commandsBuffer(RendererBuilder::CreateCommandBuffer(recordingInfo.commandsBufferName, recordingInfo.commandBufferDef))
	, m_state(ECommandsRecorderState::Invalid)
{ }

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
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(IsRecording());

	barrier.GetRHI().Execute(m_commandsBuffer->GetRHI());
}

void CommandsRecorder::BeginRendering(const RenderingDefinition& definition)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(IsRecording());

	m_commandsBuffer->GetRHI().BeginRendering(definition.GetRHI());
}

void CommandsRecorder::EndRendering()
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(IsRecording());

	m_commandsBuffer->GetRHI().EndRendering();
}

void CommandsRecorder::InitializeUIFonts(const lib::SharedPtr<rdr::UIBackend>& uiBackend)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(IsRecording());
	SPT_CHECK(!!uiBackend);

	uiBackend->GetRHI().InitializeFonts(m_commandsBuffer->GetRHI());
}

void CommandsRecorder::RenderUI(const lib::SharedPtr<rdr::UIBackend>& uiBackend)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(IsRecording());
	SPT_CHECK(!!uiBackend);

	uiBackend->GetRHI().Render(m_commandsBuffer->GetRHI());
}

}
