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
class PipelinesCache;
class SamplersCache;
class Window;
class RenderContext;
class DescriptorHeap;
class DescriptorManager;
class DescriptorSetLayout;
class TransfersManager;
class DescriptorSetStateLayoutsRegistry;

struct GPUApiData;


using OnRendererInitializedDelegate = lib::ThreadSafeMulticastDelegate<void()>;
using OnRendererCleanupDelegate     = lib::ThreadSafeMulticastDelegate<void()>;


struct GlobalShaderParam
{
	Bool IsValid() const { return type.IsValid() && address != 0u; }

	lib::HashedString  type;
	rhi::DeviceAddress address = 0u;
};


enum class EDeferredReleasesFlushFlags : Flags32
{
	None      = 0u,
	Immediate = BIT(1u),

	Default = None
};


class RENDERER_CORE_API GPUApi
{
public:

	static void									Initialize();
	static void									Uninitialize();

	static GPUApiData*							GetGPUApiData();
	static void									InitializeModule(GPUApiData& data);

	static void									FlushCaches();

	static ShadersManager&						GetShadersManager();

	static PipelinesCache&						GetPipelinesCache();
	
	static DeviceQueuesManager&					GetDeviceQueuesManager();

	static DescriptorHeap&						GetResourceDescriptorHeap();
	static DescriptorHeap&						GetSamplerDescriptorHeap();
	static DescriptorManager&					GetDescriptorManager();

	static TransfersManager&					GetTransfersManager();

	static const lib::SharedPtr<DescriptorSetLayout>&	GetShaderParamsDSLayout();

	static void									ReleaseDeferred(GPUReleaseQueue::ReleaseEntry entry);
	static void									ScheduleFlushDeferredReleases(EDeferredReleasesFlushFlags flags = EDeferredReleasesFlushFlags::Default);
	static void									FlushReadyReleases();

	static void									FlushPendingEvents();

	static void									WaitIdle(Bool releaseRuntimeResources = true);

	// Can be used before GPUApi is initialized. Broadcasted at the end of Initialize()
	static OnRendererInitializedDelegate&		GetOnRendererInitializedDelegate();
	static OnRendererCleanupDelegate&			GetOnRendererCleanupDelegate();

	static void									SetGlobalShaderParam(const GlobalShaderParam& param);
	static const GlobalShaderParam&				GetGlobalShaderParam();

	static Bool IsRayTracingEnabled();

#if WITH_SHADERS_HOT_RELOAD
	static void HotReloadShaders();
#endif // WITH_SHADERS_HOT_RELOAD

private:

	// block creating instance
	GPUApi() = default;
};

#define RENDERER_DISABLE_VALIDATION_WARNINGS_SCOPE RHI_DISABLE_VALIDATION_WARNINGS_SCOPE

} // spt::rdr
