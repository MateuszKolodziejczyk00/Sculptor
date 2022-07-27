#pragma once

#include "RendererCoreMacros.h"
#include "SculptorCoreTypes.h"
#include "RHICommandBufferTypes.h"


namespace spt::renderer
{

class CommandBuffer;


struct CommandsRecordingInfo
{
	lib::HashedString					m_commandsBufferName;
	rhi::CommandBufferDefinition		m_commandBufferDef;
	rhi::CommandBufferUsageDefinition	m_commandBufferUsage;
};


class RENDERER_CORE_API CommandsRecorder
{
public:

	CommandsRecorder(const CommandsRecordingInfo& recordingInfo);
	~CommandsRecorder();

	Bool								IsRecording() const;

	void								FinishRecording();

private:

	lib::SharedPtr<CommandBuffer>		m_commandsBuffer;
};

}
