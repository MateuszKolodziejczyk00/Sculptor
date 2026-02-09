#include "Renderer.h"
#include "RendererSettings.h"
#include "CommandsRecorder/CommandRecorder.h"
#include "Types/Semaphore.h"
#include "Types/CommandBuffer.h"
#include "Types/Window.h"
#include "Shaders/ShadersManager.h"
#include "Pipelines/PipelinesCache.h"
#include "Samplers/SamplersCache.h"
#include "GPUDiagnose/Diagnose.h"
#include "Window/PlatformWindowImpl.h"
#include "RHICore/RHIInitialization.h"
#include "RHICore/RHISubmitTypes.h"
#include "RHIBridge/RHISemaphoreImpl.h"
#include "RHIBridge/RHICommandBufferImpl.h"
#include "EngineFrame.h"
#include "JobSystem.h"
#include "ResourcesManager.h"
#include "Types/DescriptorSetState/DescriptorManager.h"


namespace spt::rdr
{

namespace priv
{

struct RendererData
{

ShadersManager shadersManager;

PipelinesCache pipelinesCache;

SamplersCache samplersCache;

OnRendererCleanupDelegate cleanupDelegate;

DeviceQueuesManager deviceQueuesManager;

GPUReleaseQueue releasesQueue;

lib::SharedPtr<DescriptorHeap> descriptorHeap;

lib::UniquePtr<DescriptorManager> descriptorsManager;

lib::SharedPtr<DescriptorSetLayout> shaderParamsDSLayout;

lib::Lock releaseQueueLock;
lib::DynamicArray<GPUReleaseQueue::ReadyQueue> readyReleases;

};

static RendererData g_data;

static void InitializeSamplerDescriptors()
{
	SPT_PROFILER_FUNCTION();

	rhi::SamplerDefinition materialAnisoSampler = rhi::SamplerState::LinearRepeat;
	materialAnisoSampler.mipLodBias       = -0.5f;
	materialAnisoSampler.enableAnisotropy = true;
	materialAnisoSampler.maxAnisotropy    = 8.f;

	rhi::SamplerDefinition materialLinearSampler = rhi::SamplerState::LinearRepeat;
	materialLinearSampler.mipLodBias       = -0.5f;

	const rhi::SamplerDefinition samplerStates[] =
	{
		rhi::SamplerState::LinearClampToEdge,
		rhi::SamplerState::NearestClampToEdge,
		rhi::SamplerState::LinearRepeat,
		rhi::SamplerState::NearestClampToEdge,
		rhi::SamplerState::LinearMinClampToEdge,
		rhi::SamplerState::LinearMaxClampToEdge,
		materialAnisoSampler,
		materialLinearSampler
	};

	for (Uint32 i = 0; i < SPT_ARRAY_SIZE(samplerStates); ++i)
	{
		const lib::SharedRef<rdr::Sampler> sampler = rdr::ResourcesManager::CreateSampler(samplerStates[i]);
		priv::g_data.descriptorsManager->UploadSamplerDescriptor(i, *sampler);
	}
}

static void InitializeShaderParamsDSLayout()
{
	rhi::DescriptorSetBindingDefinition bindingDef;
	bindingDef.bindingIdx      = 0u;
	bindingDef.descriptorType  = rhi::EDescriptorType::UniformBuffer;
	bindingDef.descriptorCount = 1u;
	bindingDef.shaderStages    = rhi::EShaderStageFlags::All;

	rhi::DescriptorSetDefinition layoutDef;
	layoutDef.bindings.emplace_back(bindingDef);
	priv::g_data.shaderParamsDSLayout = rdr::ResourcesManager::CreateDescriptorSetLayout(RENDERER_RESOURCE_NAME("Shader Params Layout"), layoutDef);
}

} // priv

void Renderer::Initialize()
{
	platf::PlatformWindow::Initialize();

	rhi::RHI::Initialize(rhi::RHIInitializationInfo{});

	const Uint64 descriptorsBufferSize = 1024u * 1024u * 32u;
	priv::g_data.descriptorHeap = ResourcesManager::CreateDescriptorHeap(RENDERER_RESOURCE_NAME("Renderer Descriptor Heap"),
																		 rhi::DescriptorHeapDefinition{ .size = descriptorsBufferSize });

	priv::g_data.descriptorsManager = std::make_unique<DescriptorManager>(*priv::g_data.descriptorHeap);

	GetShadersManager().Initialize();

	GetSamplersCache().Initialize();

	GetDeviceQueuesManager().Initialize();

	DescriptorSetStateLayoutsRegistry::Get().CreateRegisteredLayouts();

	priv::InitializeShaderParamsDSLayout();

	priv::InitializeSamplerDescriptors();
}

void Renderer::Uninitialize()
{
	SPT_PROFILER_FUNCTION();

	WaitIdle();

	priv::g_data.shaderParamsDSLayout.reset();

	DescriptorSetStateLayoutsRegistry::Get().ReleaseRegisteredLayouts();

	ScheduleFlushDeferredReleases(EDeferredReleasesFlushFlags::Immediate);

	GetOnRendererCleanupDelegate().Broadcast();

	GetDeviceQueuesManager().Uninitialize();

	GetSamplersCache().Uninitialize();

	GetPipelinesCache().ClearCachedPipelines();

	GetShadersManager().Uninitialize();

	ScheduleFlushDeferredReleases(EDeferredReleasesFlushFlags::Immediate);

	priv::g_data.descriptorsManager.reset();

	priv::g_data.descriptorHeap.reset();

	ScheduleFlushDeferredReleases(EDeferredReleasesFlushFlags::Immediate);

	rhi::RHI::Uninitialize();
}

void Renderer::FlushCaches()
{
	SPT_PROFILER_FUNCTION();
	
	rhi::RHI::FlushCaches();

	GetSamplersCache().FlushPendingSamplers();
	
	GetPipelinesCache().FlushCreatedPipelines();
}

ShadersManager& Renderer::GetShadersManager()
{
	return priv::g_data.shadersManager;
}

PipelinesCache& Renderer::GetPipelinesCache()
{
	return priv::g_data.pipelinesCache;
}

SamplersCache& Renderer::GetSamplersCache()
{
	return priv::g_data.samplersCache;
}

DeviceQueuesManager& Renderer::GetDeviceQueuesManager()
{
	return priv::g_data.deviceQueuesManager;
}

DescriptorHeap& Renderer::GetDescriptorHeap()
{
	return *priv::g_data.descriptorHeap;
}

DescriptorManager& Renderer::GetDescriptorManager()
{
	SPT_CHECK(!!priv::g_data.descriptorsManager);

	return *priv::g_data.descriptorsManager;
}

const lib::SharedPtr<DescriptorSetLayout>& Renderer::GetShaderParamsDSLayout()
{
	return priv::g_data.shaderParamsDSLayout;
}

void Renderer::ReleaseDeferred(GPUReleaseQueue::ReleaseEntry entry)
{
	priv::g_data.releasesQueue.Enqueue(std::move(entry));
}

void Renderer::ScheduleFlushDeferredReleases(EDeferredReleasesFlushFlags flags /*= EDeferredReleasesFlushFlags::Default*/)
{
	auto flushLambda = [queue = std::move(priv::g_data.releasesQueue.Flush())]() mutable
	{
		lib::LockGuard lock(priv::g_data.releaseQueueLock);
		priv::g_data.readyReleases.emplace_back(std::move(queue));
	};

	if (lib::HasAnyFlag(flags, EDeferredReleasesFlushFlags::Immediate))
	{
		GetDeviceQueuesManager().FlushSubmittedWorkloads();

		flushLambda();

		FlushReadyReleases();
	}
	else
	{
		const js::Event gpuFlushedEvent = js::CreateEvent("On GPU Finished");

		GetDeviceQueuesManager().SignalAfterFlushingPendingWork(gpuFlushedEvent);

		js::Launch("Flush Deferred Releases", std::move(flushLambda), js::Prerequisites(gpuFlushedEvent));
	}
}

void Renderer::FlushReadyReleases()
{
	SPT_PROFILER_FUNCTION();

	lib::DynamicArray<GPUReleaseQueue::ReadyQueue> readyReleases;

	{
		lib::LockGuard lock(priv::g_data.releaseQueueLock);
		std::swap(readyReleases, priv::g_data.readyReleases);
	}

	for (GPUReleaseQueue::ReadyQueue& queue : readyReleases)
	{
		queue.Broadcast();
	}
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
