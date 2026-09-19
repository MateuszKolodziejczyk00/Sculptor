#include "GPUApi.h"
#include "RendererSettings.h"
#include "CommandsRecorder/CommandRecorder.h"
#include "Types/Semaphore.h"
#include "Types/CommandBuffer.h"
#include "Types/Window.h"
#include "Shaders/ShadersManager.h"
#include "Pipelines/PipelinesCache.h"
#include "RHICore/RHIInitialization.h"
#include "JobSystem.h"
#include "ResourcesManager.h"
#include "Descriptors/DescriptorManager.h"
#include "Pipelines/PSOsLibrary.h"
#include "Utils/TransfersManager.h"


namespace spt::rdr
{

struct GPUApiData
{
	ShadersManager shadersManager;
	
	PipelinesCache pipelinesCache;

	TransfersManager* transfersManager;
	
	OnRendererCleanupDelegate cleanupDelegate;
	
	DeviceQueuesManager deviceQueuesManager;
	
	GPUReleaseQueue releasesQueue;
	
	lib::SharedPtr<DescriptorHeap> resourceDescriptorHeap;
	lib::SharedPtr<DescriptorHeap> samplerDescriptorHeap;
	
	lib::UniquePtr<DescriptorManager> descriptorsManager;
	
	lib::Lock releaseQueueLock;
	lib::DynamicArray<GPUReleaseQueue::ReadyQueue> readyReleases;

	rhi::RHIModuleData* rhiModuleData = nullptr;

	StructsRegistryData* shaderStructsRegistryData = nullptr;

	GPUApiFactoryData* gpuApiFactoryData = nullptr;
};

static GPUApiData* g_GPUApiData = nullptr;

namespace utils
{

static void InitializeSamplerDescriptors()
{
	SPT_PROFILER_FUNCTION();

	rhi::SamplerDefinition materialAnisoSampler = rhi::SamplerState::LinearRepeat;
	materialAnisoSampler.mipLodBias       = -0.5f;
	materialAnisoSampler.enableAnisotropy = true;
	materialAnisoSampler.maxAnisotropy    = 8.f;

	rhi::SamplerDefinition materialLinearSampler = rhi::SamplerState::LinearRepeat;
	materialLinearSampler.mipLodBias       = -0.5f;

	rhi::SamplerDefinition samplerStates[] =
	{
		rhi::SamplerState::LinearClampToEdge,
		rhi::SamplerState::NearestClampToEdge,
		rhi::SamplerState::LinearRepeat,
		rhi::SamplerState::NearestRepeat,
		rhi::SamplerState::LinearMinClampToEdge,
		rhi::SamplerState::LinearMaxClampToEdge,
		materialAnisoSampler,
		materialLinearSampler
	};

	for (Uint32 i = 0; i < SPT_ARRAY_SIZE(samplerStates); ++i)
	{
		g_GPUApiData->descriptorsManager->UploadSamplerDescriptor(i, samplerStates[i]);
	}
}

} // utils

void GPUApi::Initialize()
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(!g_GPUApiData);

	g_GPUApiData = new GPUApiData();

	g_GPUApiData->gpuApiFactoryData = ResourcesManager::Initialize();

	platf::PlatformWindow::Initialize();

	rhi::RHI::Initialize(rhi::RHIInitializationInfo{});

	const rhi::DescriptorProps& descriptorProps = rhi::RHI::GetDescriptorProps();

	g_GPUApiData->resourceDescriptorHeap = ResourcesManager::CreateDescriptorHeap(RENDERER_RESOURCE_NAME("GPUApi Resource Descriptor Heap"),
																		 rhi::DescriptorHeapDefinition{ .size = descriptorProps.maxResourceHeapSize, .type = rhi::EDescriptorHeapType::Resource });

	g_GPUApiData->samplerDescriptorHeap = ResourcesManager::CreateDescriptorHeap(RENDERER_RESOURCE_NAME("GPUApi Sampler Descriptor Heap"),
																		 rhi::DescriptorHeapDefinition{ .size = descriptorProps.maxSamplerHeapSize, .type = rhi::EDescriptorHeapType::Sampler });


	g_GPUApiData->descriptorsManager = std::make_unique<DescriptorManager>(*g_GPUApiData->resourceDescriptorHeap, *g_GPUApiData->samplerDescriptorHeap);

	GetShadersManager().Initialize();

	GetDeviceQueuesManager().Initialize();

	utils::InitializeSamplerDescriptors();

	g_GPUApiData->transfersManager = new TransfersManager();

	g_GPUApiData->rhiModuleData = rhi::RHI::GetModuleData();
	g_GPUApiData->shaderStructsRegistryData = ShaderStructsRegistry::GetRegistryData();
}

void GPUApi::Uninitialize()
{
	SPT_PROFILER_FUNCTION();

	WaitIdle();

	ScheduleFlushDeferredReleases(EDeferredReleasesFlushFlags::Immediate);

	GetOnRendererCleanupDelegate().Broadcast();

	GetDeviceQueuesManager().Uninitialize();

	GetPipelinesCache().ClearCachedPipelines();

	GetShadersManager().Uninitialize();

	ScheduleFlushDeferredReleases(EDeferredReleasesFlushFlags::Immediate);

	g_GPUApiData->descriptorsManager.reset();

	g_GPUApiData->samplerDescriptorHeap.reset();

	g_GPUApiData->resourceDescriptorHeap.reset();

	ScheduleFlushDeferredReleases(EDeferredReleasesFlushFlags::Immediate);

	rhi::RHI::Uninitialize();
}

GPUApiData* GPUApi::GetGPUApiData()
{
	SPT_CHECK(!!g_GPUApiData);

	return g_GPUApiData;
}

void GPUApi::InitializeModule(GPUApiData& data)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(!g_GPUApiData);

	g_GPUApiData = &data;

	ResourcesManager::InitializeModule(g_GPUApiData->gpuApiFactoryData);

	rhi::RHI::InitializeModule(g_GPUApiData->rhiModuleData);

	g_GPUApiData->shadersManager.InitializeModule();

	ShaderStructsRegistry::InitializeModule(g_GPUApiData->shaderStructsRegistryData);

	PSOPrecacheParams precacheParams{};
	PSOsLibrary::GetInstance().PrecachePSOs(precacheParams);

	g_GPUApiData->shadersManager.HotReloadShaders();
}

void GPUApi::FlushCaches()
{
	SPT_PROFILER_FUNCTION();
	
	GetPipelinesCache().FlushCreatedPipelines();
}

ShadersManager& GPUApi::GetShadersManager()
{
	return g_GPUApiData->shadersManager;
}

PipelinesCache& GPUApi::GetPipelinesCache()
{
	return g_GPUApiData->pipelinesCache;
}

DeviceQueuesManager& GPUApi::GetDeviceQueuesManager()
{
	return g_GPUApiData->deviceQueuesManager;
}

DescriptorHeap& GPUApi::GetResourceDescriptorHeap()
{
	return *g_GPUApiData->resourceDescriptorHeap;
}

DescriptorHeap& GPUApi::GetSamplerDescriptorHeap()
{
	return *g_GPUApiData->samplerDescriptorHeap;
}

DescriptorManager& GPUApi::GetDescriptorManager()
{
	SPT_CHECK(!!g_GPUApiData->descriptorsManager);

	return *g_GPUApiData->descriptorsManager;
}

TransfersManager& GPUApi::GetTransfersManager()
{
	return *g_GPUApiData->transfersManager;
}

void GPUApi::ReleaseDeferred(GPUReleaseQueue::ReleaseEntry entry)
{
	g_GPUApiData->releasesQueue.Enqueue(std::move(entry));
}

void GPUApi::ScheduleFlushDeferredReleases(EDeferredReleasesFlushFlags flags /*= EDeferredReleasesFlushFlags::Default*/)
{
	auto flushLambda = [queue = std::move(g_GPUApiData->releasesQueue.Flush())]() mutable
	{
		lib::LockGuard lock(g_GPUApiData->releaseQueueLock);
		g_GPUApiData->readyReleases.emplace_back(std::move(queue));
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

void GPUApi::FlushReadyReleases()
{
	SPT_PROFILER_FUNCTION();

	lib::DynamicArray<GPUReleaseQueue::ReadyQueue> readyReleases;

	{
		lib::LockGuard lock(g_GPUApiData->releaseQueueLock);
		std::swap(readyReleases, g_GPUApiData->readyReleases);
	}

	for (GPUReleaseQueue::ReadyQueue& queue : readyReleases)
	{
		queue.Broadcast();
	}
}

void GPUApi::FlushPendingEvents()
{
	GetDeviceQueuesManager().FlushSubmittedWorkloads();
}

void GPUApi::WaitIdle(Bool releaseRuntimeResources /*= true*/)
{
	SPT_PROFILER_FUNCTION();

	rhi::RHI::WaitIdle();

	FlushPendingEvents();

	if (releaseRuntimeResources)
	{
		ScheduleFlushDeferredReleases(EDeferredReleasesFlushFlags::Immediate);
	}
}

OnRendererCleanupDelegate& GPUApi::GetOnRendererCleanupDelegate()
{
	return g_GPUApiData->cleanupDelegate;
}

Bool GPUApi::IsRayTracingEnabled()
{
	return rhi::RHI::IsRayTracingEnabled();
}

#if WITH_SHADERS_HOT_RELOAD
void GPUApi::HotReloadShaders()
{
	SPT_PROFILER_FUNCTION();

	GetShadersManager().HotReloadShaders();
}
#endif // WITH_SHADERS_HOT_RELOAD

} // spt::rdr
