#pragma once

#include "RendererCoreMacros.h"
#include "SculptorCoreTypes.h"
#include "RendererUtils.h"
#include "RHICore/RHICommandBufferTypes.h"
#include "RHIBridge/RHIImpl.h"
#include "Delegates/MulticastDelegate.h"
#include "DeviceQueues/DeviceQueuesManager.h"
#include "Types/Window.h"
#include "GPUReleaseQueue.h"


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


using OnRendererCleanupDelegate = lib::ThreadSafeMulticastDelegate<void()>;


enum class EDeferredReleasesFlushFlags : Flags32
{
	None      = 0u,
	Immediate = BIT(1u),

	Default = None
};


class RENDERER_CORE_API Renderer
{
public:

	static void									Initialize();

	static void									Uninitialize();

	static void									FlushCaches();

	static ShadersManager&						GetShadersManager();

	static PipelinesLibrary&					GetPipelinesLibrary();

	static SamplersCache&						GetSamplersCache();
	
	static DeviceQueuesManager&					GetDeviceQueuesManager();

	static void									ReleaseDeferred(GPUReleaseQueue::ReleaseEntry entry);
	static void									ScheduleFlushDeferredReleases(EDeferredReleasesFlushFlags flags = EDeferredReleasesFlushFlags::Default);

	static void									PresentTexture(const lib::SharedRef<Window>& window, rdr::SwapchainTextureHandle swapchainTexture, const lib::DynamicArray<lib::SharedPtr<Semaphore>>& waitSemaphores);

	static void									FlushPendingEvents();

	static void									WaitIdle(Bool releaseRuntimeResources = true);

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