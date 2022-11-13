#include "Vulkan/Debug/GPUCrashTracker.h"
#include "FileSystem/File.h"
#include "Paths.h"

#if WITH_NSIGHT_AFTERMATH
#include "GFSDK_Aftermath_GpuCrashDump.h"
#include "GFSDK_Aftermath_GpuCrashDumpDecoding.h"
#endif // WITH_NSIGHT_AFTERMATH

#include <ctime>
#include <iomanip>

namespace spt::vulkan
{

SPT_DEFINE_LOG_CATEGORY(GPUCrashTracker, true)

#if WITH_NSIGHT_AFTERMATH

#define AFTERMATH_CHECK(expr) { const GFSDK_Aftermath_Result result = expr; SPT_CHECK(GFSDK_Aftermath_SUCCEED(result)); }

class GPUCrashTrackerInstance
{
public:

    void GPUCrashDump(const void* pGpuCrashDump, const uint32_t gpuCrashDumpSize);

    void CrashDumpDescription(PFN_GFSDK_Aftermath_AddGpuCrashDumpDescription addDescription, void* userData);

private:


    lib::String GenerateDumpFileNameWithoutExtension() const;
};

void GPUCrashTrackerInstance::GPUCrashDump(const void* gpuCrashDump, const uint32_t gpuCrashDumpSize)
{
    SPT_PROFILER_FUNCTION();

    // Create a GPU crash dump decoder object for the GPU crash dump.
    GFSDK_Aftermath_GpuCrashDump_Decoder decoder = {};
    AFTERMATH_CHECK(GFSDK_Aftermath_GpuCrashDump_CreateDecoder(GFSDK_Aftermath_Version_API,
                                                               gpuCrashDump,
                                                               gpuCrashDumpSize,
                                                               &decoder));

    // Use the decoder object to read basic information, like application
    // name, PID, etc. from the GPU crash dump.
    GFSDK_Aftermath_GpuCrashDump_BaseInfo baseInfo = {};
    AFTERMATH_CHECK(GFSDK_Aftermath_GpuCrashDump_GetBaseInfo(decoder, &baseInfo));

    // Use the decoder object to query the application name that was set
    // in the GPU crash dump description.
    Uint32 applicationNameLength = 0;
    AFTERMATH_CHECK(GFSDK_Aftermath_GpuCrashDump_GetDescriptionSize(decoder,
                                                                    GFSDK_Aftermath_GpuCrashDumpDescriptionKey_ApplicationName,
                                                                    &applicationNameLength));

    lib::DynamicArray<char> applicationName(applicationNameLength, '\0');

    AFTERMATH_CHECK(GFSDK_Aftermath_GpuCrashDump_GetDescription(decoder,
                                                                GFSDK_Aftermath_GpuCrashDumpDescriptionKey_ApplicationName,
                                                                static_cast<Uint32>(applicationName.size()),
                                                                applicationName.data()));

    const lib::String dumpFilePathNoExtension = GenerateDumpFileNameWithoutExtension();
    SPT_CHECK(!dumpFilePathNoExtension.empty());

    const lib::String dumpFilePath = dumpFilePathNoExtension + ".nv-gpudmp";

	std::ofstream dumpStream = lib::File::OpenOutputStream(dumpFilePath, lib::Flags(lib::EFileOpenFlags::Binary, lib::EFileOpenFlags::DiscardContent, lib::EFileOpenFlags::ForceCreate));
    if (dumpStream)
    {
        dumpStream.write(static_cast<const char*>(gpuCrashDump), gpuCrashDumpSize);
        dumpStream.close();
    }

    /*
    // Decode the crash dump to a JSON string.
    // Step 1: Generate the JSON and get the size.
    Uint32 jsonSize = 0;
    AFTERMATH_CHECK(GFSDK_Aftermath_GpuCrashDump_GenerateJSON(decoder,
                                                              GFSDK_Aftermath_GpuCrashDumpDecoderFlags_ALL_INFO,
                                                              GFSDK_Aftermath_GpuCrashDumpFormatterFlags_NONE,
                                                              ShaderDebugInfoLookupCallback,
                                                              ShaderLookupCallback,
                                                              ShaderSourceDebugInfoLookupCallback,
                                                              this,
                                                              &jsonSize));
    // Step 2: Allocate a buffer and fetch the generated JSON.
    lib::DynamicArray<char> json(jsonSize);
    AFTERMATH_CHECK(GFSDK_Aftermath_GpuCrashDump_GetJSON(decoder,
                                                         static_cast<Uint32>(json.size()),
                                                         json.data()));

    // Write the crash dump data as JSON to a file.
    const std::string jsonFileName = dumpFilePathNoExtension  + ".json";
	std::ofstream jsonStream = lib::File::OpenOutputStream(jsonFileName, lib::Flags(lib::EFileOpenFlags::Binary, lib::EFileOpenFlags::DiscardContent, lib::EFileOpenFlags::ForceCreate));
    if (jsonStream)
    {
       // Write the JSON to the file (excluding string termination)
       jsonStream.write(json.data(), json.size() - 1);
       jsonStream.close();
    }
    */
    // Destroy the GPU crash dump decoder object.
    AFTERMATH_CHECK(GFSDK_Aftermath_GpuCrashDump_DestroyDecoder(decoder));

}

void GPUCrashTrackerInstance::CrashDumpDescription(PFN_GFSDK_Aftermath_AddGpuCrashDumpDescription addDescription, void* userData)
{
    SPT_PROFILER_FUNCTION();

    addDescription(GFSDK_Aftermath_GpuCrashDumpDescriptionKey_ApplicationName, "Sculptor");
    addDescription(GFSDK_Aftermath_GpuCrashDumpDescriptionKey_ApplicationVersion, "v0.1");
    addDescription(GFSDK_Aftermath_GpuCrashDumpDescriptionKey_UserDefined, "GPU Crash Dump");
}

lib::String GPUCrashTrackerInstance::GenerateDumpFileNameWithoutExtension() const
{
	const std::time_t currentTime = std::time(nullptr);

	std::tm localTime{};
	localtime_s(&localTime, &currentTime);

	std::ostringstream oss;
	oss << std::put_time(&localTime, "%d%m%Y_%H%M%S");
	const lib::String timeString = oss.str();

	return engn::Paths::Combine(engn::Paths::GetGPUCrashDumpsPath(), timeString);
}

GPUCrashTrackerInstance trackerInstnace;

static void GpuCrashDumpCallback(const void* gpuCrashDump, const uint32_t gpuCrashDumpSize, void* userData)
{
    GPUCrashTrackerInstance* gpuCrashTracker = reinterpret_cast<GPUCrashTrackerInstance*>(userData);
    gpuCrashTracker->GPUCrashDump(gpuCrashDump, gpuCrashDumpSize);
	SPT_LOG_INFO(GPUCrashTracker, "GpuCrashDumpCallback");
}

static void ShaderDebugInfoCallback(const void* shaderDebugInfo, const uint32_t shaderDebugInfoSize, void* userData)
{
    //GPUCrashTrackerInstance* gpuCrashTracker = reinterpret_cast<GPUCrashTrackerInstance*>(userData);
	SPT_LOG_INFO(GPUCrashTracker, "ShaderDebugInfoCallback");
}

static void CrashDumpDescriptionCallback(PFN_GFSDK_Aftermath_AddGpuCrashDumpDescription addDescription, void* userData)
{
    GPUCrashTrackerInstance* gpuCrashTracker = reinterpret_cast<GPUCrashTrackerInstance*>(userData);
    gpuCrashTracker->CrashDumpDescription(addDescription, userData);
}

static void ResolveMarkerCallback(const void* marker, void* userData, void** resolvedMarkerData, uint32_t* markerSize)
{
    //GPUCrashTrackerInstance* gpuCrashTracker = reinterpret_cast<GPUCrashTrackerInstance*>(userData);
	SPT_LOG_INFO(GPUCrashTracker, "ResolveMarkerCallback");
}
#endif // WITH_NSIGHT_AFTERMATH

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
#endif // WITH_NSIGHT_AFTERMATH
}

void GPUCrashTracker::SaveGPUCrashDump()
{
	SPT_PROFILER_FUNCTION();

#if WITH_NSIGHT_AFTERMATH
	GFSDK_Aftermath_CrashDump_Status status = GFSDK_Aftermath_CrashDump_Status_Unknown;

    AFTERMATH_CHECK(GFSDK_Aftermath_GetCrashDumpStatus(&status));

    const auto startTimestamp = std::chrono::steady_clock::now();
    auto elapsedTime = std::chrono::milliseconds::zero();

    constexpr SizeType timeoutInMilliseconds = 5000;

    while (status != GFSDK_Aftermath_CrashDump_Status_CollectingDataFailed
        && status != GFSDK_Aftermath_CrashDump_Status_Finished
        &&  elapsedTime.count() < timeoutInMilliseconds)
    {
        // Sleep a couple of milliseconds and poll the status again.
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        AFTERMATH_CHECK(GFSDK_Aftermath_GetCrashDumpStatus(&status));

        elapsedTime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startTimestamp);
    }

    if (status == GFSDK_Aftermath_CrashDump_Status_Finished)
    {
        SPT_LOG_WARN(GPUCrashTracker, "Aftermath finished processing the crash dump.");
    }
    else
    {
        SPT_LOG_FATAL(GPUCrashTracker, "Unexpected crash dump status after timeout: {0}", status);
    }
#endif // WITH_NSIGHT_AFTERMATH
}

} // spt::vulkan
