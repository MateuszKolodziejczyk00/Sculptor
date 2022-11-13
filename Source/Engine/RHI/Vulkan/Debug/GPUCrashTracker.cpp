#include "Vulkan/Debug/GPUCrashTracker.h"

#if WITH_NSIGHT_AFTERMATH
#include "GFSDK_Aftermath_GpuCrashDump.h"
#endif // WITH_NSIGHT_AFTERMATH

namespace spt::vulkan
{

SPT_DEFINE_LOG_CATEGORY(GPUCrashTracker, true)

class GPUCrashTrackerInstance
{
public:

private:


};

#if WITH_NSIGHT_AFTERMATH
GPUCrashTrackerInstance trackerInstnace;

static void GpuCrashDumpCallback(const void* pGpuCrashDump, const uint32_t gpuCrashDumpSize, void* pUserData)
{
    //GPUCrashTrackerInstance* gpuCrashTracker = reinterpret_cast<GPUCrashTrackerInstance*>(pUserData);
	SPT_LOG_INFO(GPUCrashTracker, "GpuCrashDumpCallback");
}

static void ShaderDebugInfoCallback(const void* pShaderDebugInfo, const uint32_t shaderDebugInfoSize, void* pUserData)
{
    //GPUCrashTrackerInstance* gpuCrashTracker = reinterpret_cast<GPUCrashTrackerInstance*>(pUserData);
	SPT_LOG_INFO(GPUCrashTracker, "ShaderDebugInfoCallback");
}

static void CrashDumpDescriptionCallback(PFN_GFSDK_Aftermath_AddGpuCrashDumpDescription addDescription, void* pUserData)
{
    //GPUCrashTrackerInstance* gpuCrashTracker = reinterpret_cast<GPUCrashTrackerInstance*>(pUserData);
	SPT_LOG_INFO(GPUCrashTracker, "CrashDumpDescriptionCallback");
}

static void ResolveMarkerCallback(const void* pMarker, void* pUserData, void** resolvedMarkerData, uint32_t* markerSize)
{
    //GPUCrashTrackerInstance* gpuCrashTracker = reinterpret_cast<GPUCrashTrackerInstance*>(pUserData);
	SPT_LOG_INFO(GPUCrashTracker, "ResolveMarkerCallback");
}
#endif WITH_NSIGHT_AFTERMATH

void GPUCrashTracker::EnableGPUCrashDumps()
{
#if WITH_NSIGHT_AFTERMATH
	const GFSDK_Aftermath_Result result = GFSDK_Aftermath_EnableGpuCrashDumps(GFSDK_Aftermath_Version_API,
																			  GFSDK_Aftermath_GpuCrashDumpWatchedApiFlags_Vulkan,
																			  GFSDK_Aftermath_GpuCrashDumpFeatureFlags_Default,
																			  &GpuCrashDumpCallback,
																			  &ShaderDebugInfoCallback,
																			  &CrashDumpDescriptionCallback,
																			  &ResolveMarkerCallback,
																			  &trackerInstnace);

	SPT_CHECK(result == GFSDK_Aftermath_Result_Success);
#endif WITH_NSIGHT_AFTERMATH
}

void GPUCrashTracker::SaveGPUCrashDump()
{

}

} // spt::vulkan
