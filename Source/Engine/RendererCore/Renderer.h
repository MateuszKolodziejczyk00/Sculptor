#pragma once

#include "RendererCoreMacros.h"
#include "SculptorCoreTypes.h"
#include "RendererUtils.h"
#include "RHICore/RHICommandBufferTypes.h"
#include "RHIBridge/RHIImpl.h"
#include "Delegates/MulticastDelegate.h"
#include "DeviceQueues/DeviceQueuesManager.h"
#include "Types/Window.h"


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


using OnRendererCleanupDelegate = lib::ThreadSafeMulticastDelegate<void()>;


class RENDERER_CORE_API Renderer
{
public:

	static void									Initialize();

	static void									Uninitialize();

	static void									BeginFrame(Uint64 frameIdx);

	static ShadersManager&						GetShadersManager();

	static PipelinesLibrary&					GetPipelinesLibrary();

	static SamplersCache&						GetSamplersCache();
	
	static DeviceQueuesManager&					GetDeviceQueuesManager();

	static void									PresentTexture(const lib::SharedRef<Window>& window, rdr::SwapchainTextureHandle swapchainTexture, const lib::DynamicArray<lib::SharedPtr<Semaphore>>& waitSemaphores);

	static void									FlushPendingEvents();

	static void									WaitIdle(Bool releaseRuntimeResources = true);
	
	static Uint64								GetCurrentFrameIdx();

	static OnRendererCleanupDelegate&			GetOnRendererCleanupDelegate();

	static Bool IsRayTracingEnabled();

#if WITH_SHADERS_HOT_RELOAD
	static void HotReloadShaders();
#endif // WITH_SHADERS_HOT_RELOAD

private:

	// block creating instance
	Renderer() = default;
};

#define RENDERER_DISABLE_VALIDATION_WARNINGS_SCOPE RHI_DISABLE_VALIDATION_WARNINGS_SCOPE

} // spt::rdr