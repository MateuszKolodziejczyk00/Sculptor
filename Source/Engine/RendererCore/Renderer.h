#pragma once

#include "RendererCoreMacros.h"
#include "SculptorCoreTypes.h"


namespace spt::renderer
{

struct CommandsRecordingInfo;
class CommandsRecorder;


struct CommandsSubmitBatch
{
	lib::DynamicArray<lib::UniquePtr<CommandsRecorder>>		m_recordedCommands;
};


class RENDERER_CORE_API Renderer
{
public:

	static void									Initialize();
	static void									Shutdown();

	static void									BeginFrame();
	static void									EndFrame();

	static lib::UniquePtr<CommandsRecorder>		StartRecordingCommands(const CommandsRecordingInfo& recordingInfo);

	static void									SubmitCommands(const CommandsSubmitBatch& submitBatch);
};

}