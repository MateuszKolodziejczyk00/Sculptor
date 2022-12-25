#pragma once

#include "RendererCoreMacros.h"
#include "SculptorCoreTypes.h"
#include "RendererUtils.h"
#include "RHICore/RHICommandBufferTypes.h"
#include "RHIBridge/RHIImpl.h"


namespace spt::rdr
{

struct CommandsRecordingInfo;
class CommandRecorder;
class ShadersManager;
class PipelinesLibrary;
class DescriptorSetsManager;
class SamplersCache;
class Window;
class RenderContext;


struct CommandsSubmitBatch
{
public:

	CommandsSubmitBatch() = default;

	Bool IsValid() const
	{
		return !recordedCommands.empty();
	}

	lib::DynamicArray<lib::UniquePtr<CommandRecorder>>		recordedCommands;
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

	static lib::UniquePtr<CommandRecorder>		StartRecordingCommands();

	static void									SubmitCommands(rhi::ECommandBufferQueueType queueType, const lib::DynamicArray<CommandsSubmitBatch>& submitBatches);

	static void									PresentTexture(const lib::SharedRef<Window>& window, const lib::DynamicArray<lib::SharedPtr<Semaphore>>& waitSemaphores);

	static void									WaitIdle();

	static Uint64								GetCurrentFrameIdx();
	static const lib::SharedPtr<Semaphore>&		GetReleaseFrameSemaphore();

	static void									IncrementReleaseSemaphoreToCurrentFrame();

#if WITH_SHADERS_HOT_RELOAD
	static void HotReloadShaders();
#endif // WITH_SHADERS_HOT_RELOAD

private:

	// block creating instance
	Renderer() = default;
};

#define RENDERER_DISABLE_VALIDATION_WARNINGS_SCOPE RHI_DISABLE_VALIDATION_WARNINGS_SCOPE

} // spt::rdr