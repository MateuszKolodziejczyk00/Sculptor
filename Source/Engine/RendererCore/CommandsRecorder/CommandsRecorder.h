#pragma once

#include "RendererCoreMacros.h"
#include "SculptorCoreTypes.h"
#include "RHICore/RHICommandBufferTypes.h"
#include "RendererUtils.h"


namespace spt::rdr
{

class CommandBuffer;
class Barrier;
class RenderingDefinition;
class UIBackend;


struct CommandsRecordingInfo
{
	RendererResourceName				commandsBufferName;
	rhi::CommandBufferDefinition		commandBufferDef;
};


enum class ECommandsRecorderState
{
	/** Recording didn't started */
	Invalid,
	/** Currently recording */
	Recording,
	/** Finished recording */
	Pending
};


class RENDERER_CORE_API CommandsRecorder
{
public:

	explicit CommandsRecorder(const CommandsRecordingInfo& recordingInfo);
	~CommandsRecorder();

	Bool									IsRecording() const;

	void									StartRecording(const rhi::CommandBufferUsageDefinition& commandBufferUsage);
	void									FinishRecording();

	const lib::SharedPtr<CommandBuffer>&	GetCommandsBuffer() const;

	void									ExecuteBarrier(Barrier& barrier);

	void									BeginRendering(const RenderingDefinition& definition);
	void									EndRendering();

	void									InitializeUIFonts(const lib::SharedPtr<rdr::UIBackend>& uiBackend);

	void									RenderUI(const lib::SharedPtr<rdr::UIBackend>& uiBackend);

private:

	lib::SharedPtr<CommandBuffer>			m_commandsBuffer;

	ECommandsRecorderState					m_state;
};

}
