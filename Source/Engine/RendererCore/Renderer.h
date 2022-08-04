#pragma once

#include "RendererCoreMacros.h"
#include "SculptorCoreTypes.h"
#include "RendererUtils.h"
#include "RHICommandBufferTypes.h"


namespace spt::renderer
{

struct CommandsRecordingInfo;
class CommandsRecorder;


struct CommandsSubmitBatch
{
public:

	lib::DynamicArray<lib::UniquePtr<CommandsRecorder>>		m_recordedCommands;
	SemaphoresArray											m_waitSemaphores;
	SemaphoresArray											m_signalSemaphores;
};


class RENDERER_CORE_API Renderer
{
public:

	static void									Initialize();
	static void									PostCreatedWindow();

	static void									Shutdown();

	static void									BeginFrame();
	static void									EndFrame();

	static lib::UniquePtr<CommandsRecorder>		StartRecordingCommands(const CommandsRecordingInfo& recordingInfo);

	static void									SubmitCommands(rhi::ECommandBufferQueueType queueType, const lib::DynamicArray<CommandsSubmitBatch>& submitBatches);

	static Uint64								GetCurrentFrameIdx();
	static const lib::SharedPtr<Semaphore>&		GetReleaseFrameSemaphore();

};

}