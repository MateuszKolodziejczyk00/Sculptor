#pragma once

#include "RendererCoreMacros.h"
#include "SculptorCoreTypes.h"
#include "RendererUtils.h"
#include "RHICore/RHICommandBufferTypes.h"


namespace spt::rdr
{

struct CommandsRecordingInfo;
class CommandsRecorder;
class ShadersManager;
class PipelinesLibrary;
class DescriptorSetsManager;
class SamplersCache;
class Window;
class Context;


struct CommandsSubmitBatch
{
public:

	lib::DynamicArray<lib::UniquePtr<CommandsRecorder>>		recordedCommands;
	SemaphoresArray											waitSemaphores;
	SemaphoresArray											signalSemaphores;
};


class RENDERER_CORE_API Renderer
{
public:

	static void									Initialize();
	static void									PostCreatedWindow();

	static void									Uninitialize();

	static void									BeginFrame();
	static void									EndFrame();

	static ShadersManager&						GetShadersManager();

	static PipelinesLibrary&					GetPipelinesLibrary();

	static DescriptorSetsManager&				GetDescriptorSetsManager();

	static SamplersCache&						GetSamplersCache();

	static lib::UniquePtr<CommandsRecorder>		StartRecordingCommands();

	static void									SubmitCommands(rhi::ECommandBufferQueueType queueType, const lib::DynamicArray<CommandsSubmitBatch>& submitBatches);

	static void									PresentTexture(const lib::SharedRef<Window>& window, const lib::DynamicArray<lib::SharedPtr<Semaphore>>& waitSemaphores);

	static void									WaitIdle();

	static Uint64								GetCurrentFrameIdx();
	static const lib::SharedPtr<Semaphore>&		GetReleaseFrameSemaphore();

	static void									IncrementReleaseSemaphoreToCurrentFrame();

	static void									EnableValidationWarnings(Bool enable);

private:

	// block creating instance
	Renderer() = default;
};

} // spt::rdr