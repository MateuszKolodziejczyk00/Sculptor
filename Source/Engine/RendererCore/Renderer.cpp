#include "Renderer.h"
#include "RendererSettings.h"
#include "CommandsRecorder/CommandRecorder.h"
#include "Types/Semaphore.h"
#include "Types/CommandBuffer.h"
#include "Types/Window.h"
#include "Shaders/ShadersManager.h"
#include "Pipelines/PipelinesLibrary.h"
#include "Samplers/SamplersCache.h"
#include "GPUDiagnose/Diagnose.h"
#include "Window/PlatformWindowImpl.h"
#include "RHICore/RHIInitialization.h"
#include "RHICore/RHISubmitTypes.h"
#include "RHIBridge/RHISemaphoreImpl.h"
#include "RHIBridge/RHICommandBufferImpl.h"
#include "EngineFrame.h"
#include "JobSystem.h"


namespace spt::rdr
{

namespace priv
{

struct RendererData
{

ShadersManager shadersManager;

PipelinesLibrary pipelinesLibrary;

SamplersCache samplersCache;

OnRendererCleanupDelegate cleanupDelegate;

DeviceQueuesManager deviceQueuesManager;

GPUReleaseQueue releasesQueue;

};

static RendererData g_data;

} // priv

void Renderer::Initialize()
{
	platf::PlatformWindow::Initialize();

	rhi::RHI::Initialize(rhi::RHIInitializationInfo{});

	GetShadersManager().Initialize();

	GetSamplersCache().Initialize();

	GetDeviceQueuesManager().Initialize();

	DescriptorSetStateLayoutsRegistry::Get().CreateRegisteredLayouts();
}

void Renderer::Uninitialize()
{
	WaitIdle();

	DescriptorSetStateLayoutsRegistry::Get().ReleaseRegisteredLayouts();

	ScheduleFlushDeferredReleases(EDeferredReleasesFlushFlags::Immediate);

	GetOnRendererCleanupDelegate().Broadcast();

	GetDeviceQueuesManager().Uninitialize();

	GetSamplersCache().Uninitialize();

	GetPipelinesLibrary().ClearCachedPipelines();

	GetShadersManager().Uninitialize();

	ScheduleFlushDeferredReleases(EDeferredReleasesFlushFlags::Immediate);

	rhi::RHI::Uninitialize();
}

void Renderer::FlushCaches()
{
	SPT_PROFILER_FUNCTION();
	
	rhi::RHI::FlushCaches();

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

SamplersCache& Renderer::GetSamplersCache()
{
	return priv::g_data.samplersCache;
}

DeviceQueuesManager& Renderer::GetDeviceQueuesManager()
{
	return priv::g_data.deviceQueuesManager;
}

void Renderer::ReleaseDeferred(GPUReleaseQueue::ReleaseEntry entry)
{
	priv::g_data.releasesQueue.Enqueue(std::move(entry));
}

void Renderer::ScheduleFlushDeferredReleases(EDeferredReleasesFlushFlags flags /*= EDeferredReleasesFlushFlags::Default*/)
{
	auto flushLambda = [queue = std::move(priv::g_data.releasesQueue.Flush())]() mutable
	{
		queue.Broadcast();
	};

	if (lib::HasAnyFlag(flags, EDeferredReleasesFlushFlags::Immediate))
	{
		GetDeviceQueuesManager().FlushSubmittedWorkloads();

		flushLambda();
	}
	else
	{
		const js::Event gpuFlushedEvent = js::CreateEvent("On GPU Finished");

		GetDeviceQueuesManager().SignalAfterFlushingPendingWork(gpuFlushedEvent);

		js::Launch("Flush Deferred Releases", std::move(flushLambda), js::Prerequisites(gpuFlushedEvent));
	}
}

void Renderer::PresentTexture(const lib::SharedRef<Window>& window, rdr::SwapchainTextureHandle swapchainTexture, const lib::DynamicArray<lib::SharedPtr<Semaphore>>& waitSemaphores)
{
	SPT_PROFILER_FUNCTION();
	
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
		ScheduleFlushDeferredReleases(EDeferredReleasesFlushFlags::Immediate);
	}
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
