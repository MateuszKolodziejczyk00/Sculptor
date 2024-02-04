#include "Renderer.h"
#include "RendererSettings.h"
#include "CommandsRecorder/CommandRecorder.h"
#include "Types/Semaphore.h"
#include "Types/CommandBuffer.h"
#include "Types/Window.h"
#include "CurrentFrameContext.h"
#include "Shaders/ShadersManager.h"
#include "Pipelines/PipelinesLibrary.h"
#include "DescriptorSets/DescriptorSetsManager.h"
#include "Samplers/SamplersCache.h"
#include "GPUDiagnose/Diagnose.h"
#include "Window/PlatformWindowImpl.h"
#include "RHICore/RHIInitialization.h"
#include "RHICore/RHISubmitTypes.h"
#include "RHIBridge/RHISemaphoreImpl.h"
#include "RHIBridge/RHICommandBufferImpl.h"
#include "EngineFrame.h"


namespace spt::rdr
{

namespace priv
{

struct RendererData
{

ShadersManager shadersManager;

PipelinesLibrary pipelinesLibrary;

DescriptorSetsManager descriptorSetsManager;

SamplersCache samplersCache;

OnRendererCleanupDelegate cleanupDelegate;

DeviceQueuesManager deviceQueuesManager;

Uint64 currentFrameIdx = 0;

};

static RendererData g_data;

} // priv

void Renderer::Initialize()
{
	platf::PlatformWindow::Initialize();

	const platf::RequiredExtensionsInfo requiredExtensions = platf::PlatformWindow::GetRequiredRHIExtensionNames();

	rhi::RHIInitializationInfo initializationInfo;
	initializationInfo.extensions		= requiredExtensions.extensions;
	initializationInfo.extensionsNum	= requiredExtensions.extensionsNum;

	rhi::RHI::Initialize(initializationInfo);

	GetShadersManager().Initialize();

	GetSamplersCache().Initialize();
}

void Renderer::PostCreatedWindow()
{
	CurrentFrameContext::Initialize(RendererSettings::Get().framesInFlight);

	GPUProfiler::Initialize();

	GetDeviceQueuesManager().Initialize();
}

void Renderer::Uninitialize()
{
	WaitIdle();

	CurrentFrameContext::ReleaseAllResources();

	GetOnRendererCleanupDelegate().Broadcast();

	GetDeviceQueuesManager().Uninitialize();

	GetSamplersCache().Uninitialize();

	GetPipelinesLibrary().ClearCachedPipelines();

	GetShadersManager().Uninitialize();

	CurrentFrameContext::Shutdown();

	rhi::RHI::Uninitialize();
}

void Renderer::BeginFrame(Uint64 frameIdx)
{
	SPT_PROFILER_FUNCTION();

	const Uint64 framesInFlight         = RendererSettings::Get().framesInFlight;
	const Uint64 renderedFramesInFlight = framesInFlight - 1;

	if (frameIdx >= renderedFramesInFlight)
	{
		CurrentFrameContext::FlushFrameReleases(frameIdx - renderedFramesInFlight);
	}

	priv::g_data.currentFrameIdx = frameIdx;
	
	rhi::RHI::BeginFrame();

	GetDescriptorSetsManager().BeginFrame();

	GetSamplersCache().FlushPendingSamplers();
	
	GetPipelinesLibrary().FlushCreatedPipelines();
}

ShadersManager& Renderer::GetShadersManager()
{
	return priv::g_data.shadersManager;
}

PipelinesLibrary& Renderer::GetPipelinesLibrary()
{
	return priv::g_data.pipelinesLibrary;
}

DescriptorSetsManager& Renderer::GetDescriptorSetsManager()
{
	return priv::g_data.descriptorSetsManager;
}

SamplersCache& Renderer::GetSamplersCache()
{
	return priv::g_data.samplersCache;
}

DeviceQueuesManager& Renderer::GetDeviceQueuesManager()
{
	return priv::g_data.deviceQueuesManager;
}

lib::UniquePtr<CommandRecorder> Renderer::StartRecordingCommands()
{
	SPT_PROFILER_FUNCTION();

	return std::make_unique<CommandRecorder>();
}

void Renderer::PresentTexture(const lib::SharedRef<Window>& window, rdr::SwapchainTextureHandle swapchainTexture, const lib::DynamicArray<lib::SharedPtr<Semaphore>>& waitSemaphores)
{
	SPT_PROFILER_FUNCTION();

	GPUProfiler::FlipFrame(window);
	
	window->PresentTexture(swapchainTexture, waitSemaphores);
}

void Renderer::FlushPendingEvents()
{
	GetDeviceQueuesManager().FlushSubmittedWorkloads();
}

void Renderer::WaitIdle(Bool releaseRuntimeResources /*= true*/)
{
	SPT_PROFILER_FUNCTION();

	rhi::RHI::WaitIdle();

	FlushPendingEvents();

	if (releaseRuntimeResources)
	{
		CurrentFrameContext::ReleaseAllResources();
	}
}

void Renderer::UpdatePersistentDescriptorSets()
{
	SPT_PROFILER_FUNCTION();

	GetDescriptorSetsManager().UpdatePersistentDescriptorSets();
}

Uint64 Renderer::GetCurrentFrameIdx()
{
	return priv::g_data.currentFrameIdx;
}

OnRendererCleanupDelegate& Renderer::GetOnRendererCleanupDelegate()
{
	return priv::g_data.cleanupDelegate;
}

Bool Renderer::IsRayTracingEnabled()
{
	return rhi::RHI::IsRayTracingEnabled();
}

#if WITH_SHADERS_HOT_RELOAD
void Renderer::HotReloadShaders()
{
	SPT_PROFILER_FUNCTION();

	GetShadersManager().HotReloadShaders();
}
#endif // WITH_SHADERS_HOT_RELOAD

} // spt::rdr
