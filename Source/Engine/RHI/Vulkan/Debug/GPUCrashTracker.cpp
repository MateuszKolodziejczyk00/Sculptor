#include "Vulkan/Debug/GPUCrashTracker.h"
#include "FileSystem/File.h"
#include "Paths.h"

#if SPT_ENABLE_NSIGHT_AFTERMATH
#include "GFSDK_Aftermath_GpuCrashDump.h"
#include "GFSDK_Aftermath_GpuCrashDumpDecoding.h"
#endif // SPT_ENABLE_NSIGHT_AFTERMATH

#include <ctime>
#include <iomanip>
#include <sstream>

//////////////////////////////////////////////////////////////////////////////////////////////////
// Helpers =======================================================================================

#if SPT_ENABLE_NSIGHT_AFTERMATH
namespace std
{
    template<typename T>
    inline std::string to_hex_string(T n)
    {
        std::stringstream stream;
        stream << std::setfill('0') << std::setw(2 * sizeof(T)) << std::hex << n;
        return stream.str();
    }

    inline std::string to_string(GFSDK_Aftermath_Result result)
    {
        return std::string("0x") + to_hex_string(static_cast<uint32_t>(result));
    }

    inline std::string to_string(const GFSDK_Aftermath_ShaderDebugInfoIdentifier& identifier)
    {
        return to_hex_string(identifier.id[0]) + "-" + to_hex_string(identifier.id[1]);
    }

    inline std::string to_string(const GFSDK_Aftermath_ShaderBinaryHash& hash)
    {
        return to_hex_string(hash.hash);
    }

    template<>
    struct hash<GFSDK_Aftermath_ShaderDebugInfoIdentifier>
    {
        size_t operator()(const GFSDK_Aftermath_ShaderDebugInfoIdentifier& idenifier) const
        {
            return spt::lib::HashCombine(idenifier.id[0], idenifier.id[1]);
        }
    };
} // std

bool operator==(GFSDK_Aftermath_ShaderDebugInfoIdentifier lhs, GFSDK_Aftermath_ShaderDebugInfoIdentifier rhs)
{
    return lhs.id[0] == rhs.id[0]
        && lhs.id[1] == rhs.id[1];
}

#endif // SPT_ENABLE_NSIGHT_AFTERMATH

namespace spt::vulkan
{

SPT_DEFINE_LOG_CATEGORY(GPUCrashTracker, true)

#if SPT_ENABLE_NSIGHT_AFTERMATH

#define AFTERMATH_CHECK(expr) { const GFSDK_Aftermath_Result result = expr; SPT_CHECK(GFSDK_Aftermath_SUCCEED(result)); }

//////////////////////////////////////////////////////////////////////////////////////////////////
// GPUCrashTrackerInstance =======================================================================

class GPUCrashTrackerInstance
{
public:

    void GPUCrashDump(const void* pGpuCrashDump, const Uint32 gpuCrashDumpSize);

    void CrashDumpDescription(PFN_GFSDK_Aftermath_AddGpuCrashDumpDescription addDescription, void* userData);

    void OnShaderDebugInfo(const void* pShaderDebugInfo, const Uint32 shaderDebugInfoSize);

    void ResolveMarker(const void* marker, void* userData, void** resolvedMarkerData, Uint32* markerSize);

private:

    lib::String GenerateDumpFileNameWithoutExtension() const;

    void WriteShaderDebugInformationToFile(GFSDK_Aftermath_ShaderDebugInfoIdentifier identifier, const void* shaderDebugInfo, const Uint32 shaderDebugInfoSize) const;

    mutable lib::Lock m_lock;

    lib::HashMap<GFSDK_Aftermath_ShaderDebugInfoIdentifier, lib::DynamicArray<Uint8>> m_shaderDebugInfo;
};

void GPUCrashTrackerInstance::GPUCrashDump(const void* gpuCrashDump, const Uint32 gpuCrashDumpSize)
{
    SPT_PROFILER_FUNCTION();

    const lib::LockGuard guard(m_lock);

    // Create a GPU crash dump decoder object for the GPU crash dump.
    GFSDK_Aftermath_GpuCrashDump_Decoder decoder = {};
    AFTERMATH_CHECK(GFSDK_Aftermath_GpuCrashDump_CreateDecoder(GFSDK_Aftermath_Version_API,
                                                               gpuCrashDump,
                                                               gpuCrashDumpSize,
                                                               &decoder));

    GFSDK_Aftermath_GpuCrashDump_PageFaultInfo pageFaultInfo{};
    const GFSDK_Aftermath_Result pageResult = GFSDK_Aftermath_GpuCrashDump_GetPageFaultInfo(decoder, &pageFaultInfo);

    if (GFSDK_Aftermath_SUCCEED(pageResult) && pageResult != GFSDK_Aftermath_Result_NotAvailable)
    {
        // Print information about the GPU page fault.
        SPT_LOG_WARN(GPUCrashTracker, "GPU page fault at {0}", pageFaultInfo.faultingGpuVA);
        SPT_LOG_WARN(GPUCrashTracker, "Fault Type: {0}", static_cast<Uint32>(pageFaultInfo.faultType));
        SPT_LOG_WARN(GPUCrashTracker, "Access Type: {0}", static_cast<Uint32>(pageFaultInfo.accessType));
        SPT_LOG_WARN(GPUCrashTracker, "Engine: {0}", static_cast<Uint32>(pageFaultInfo.engine));
        SPT_LOG_WARN(GPUCrashTracker, "Client: {0}", static_cast<Uint32>(pageFaultInfo.client));
        if (pageFaultInfo.bHasResourceInfo)
        {
            SPT_LOG_WARN(GPUCrashTracker, "Fault in resource starting at {0}", pageFaultInfo.resourceInfo.gpuVa);
            SPT_LOG_WARN(GPUCrashTracker, "Size of resource: ");
            SPT_LOG_WARN(GPUCrashTracker, "Width: {0}", pageFaultInfo.resourceInfo.width);
            SPT_LOG_WARN(GPUCrashTracker, "Height: {0}", pageFaultInfo.resourceInfo.height);
            SPT_LOG_WARN(GPUCrashTracker, "Depth: {0}", pageFaultInfo.resourceInfo.depth);
            SPT_LOG_WARN(GPUCrashTracker, "MipLevels: {0}", pageFaultInfo.resourceInfo.mipLevels);
            SPT_LOG_WARN(GPUCrashTracker, "Size: {0}", pageFaultInfo.resourceInfo.size);
            SPT_LOG_WARN(GPUCrashTracker, "Format of resource: {0}", pageFaultInfo.resourceInfo.format);
            SPT_LOG_WARN(GPUCrashTracker, "Resource was destroyed: {0}", pageFaultInfo.resourceInfo.bWasDestroyed);
        }
        else
        {
            SPT_LOG_WARN(GPUCrashTracker, "No resource info");
        }
    }

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


    // Decode the crash dump to a JSON string.
    // Step 1: Generate the JSON and get the size.
    Uint32 jsonSize = 0;
    AFTERMATH_CHECK(GFSDK_Aftermath_GpuCrashDump_GenerateJSON(decoder,
                                                              GFSDK_Aftermath_GpuCrashDumpDecoderFlags_ALL_INFO,
                                                              GFSDK_Aftermath_GpuCrashDumpFormatterFlags_NONE,
                                                              nullptr,
                                                              nullptr,
                                                              nullptr,
                                                              nullptr,
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

    // Destroy the GPU crash dump decoder object.
    AFTERMATH_CHECK(GFSDK_Aftermath_GpuCrashDump_DestroyDecoder(decoder));

}

void GPUCrashTrackerInstance::CrashDumpDescription(PFN_GFSDK_Aftermath_AddGpuCrashDumpDescription addDescription, void* userData)
{
    SPT_PROFILER_FUNCTION();

    const lib::LockGuard guard(m_lock);

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

void GPUCrashTrackerInstance::OnShaderDebugInfo(const void* shaderDebugInfo, const Uint32 shaderDebugInfoSize)
{
    SPT_PROFILER_FUNCTION();

    const lib::LockGuard guard(m_lock);

    // Get shader debug information identifier
    GFSDK_Aftermath_ShaderDebugInfoIdentifier identifier = {};
    AFTERMATH_CHECK(GFSDK_Aftermath_GetShaderDebugInfoIdentifier(GFSDK_Aftermath_Version_API,
                                                                 shaderDebugInfo,
                                                                 shaderDebugInfoSize,
                                                                 &identifier));

    // Store information for decoding of GPU crash dumps with shader address mapping
    // from within the application.
    lib::DynamicArray<Uint8> data(reinterpret_cast<const Uint8*>(shaderDebugInfo), reinterpret_cast<const Uint8*>(shaderDebugInfo) + shaderDebugInfoSize);
    m_shaderDebugInfo[identifier] = std::move(data);

    // Write to file for later in-depth analysis of crash dumps with Nsight Graphics
    WriteShaderDebugInformationToFile(identifier, shaderDebugInfo, shaderDebugInfoSize);

}

void GPUCrashTrackerInstance::ResolveMarker(const void* marker, void* userData, void** resolvedMarkerData, Uint32* markerSize)
{
    SPT_PROFILER_FUNCTION();

    const lib::HashedString::KeyType key = reinterpret_cast<lib::HashedString::KeyType>(marker);
    const lib::HashedString markerString(key);

    *resolvedMarkerData = const_cast<char*>(markerString.GetData());
    *markerSize = static_cast<Uint32>(markerString.GetSize());
}

void GPUCrashTrackerInstance::WriteShaderDebugInformationToFile(GFSDK_Aftermath_ShaderDebugInfoIdentifier identifier, const void* shaderDebugInfo, const Uint32 shaderDebugInfoSize) const
{
    const lib::String filePath = engn::Paths::Combine(engn::Paths::GetGPUCrashDumpsPath(), "shader-" + std::to_string(identifier) + ".nvdbg");

    std::ofstream stream = lib::File::OpenOutputStream(filePath, lib::Flags(lib::EFileOpenFlags::Binary, lib::EFileOpenFlags::DiscardContent, lib::EFileOpenFlags::ForceCreate));
    if (stream)
    {
        stream.write(reinterpret_cast<const char*>(shaderDebugInfo), shaderDebugInfoSize);
        stream.close();
    }
}

GPUCrashTrackerInstance trackerInstnace;

//////////////////////////////////////////////////////////////////////////////////////////////////
// Callbacks =====================================================================================

static void GpuCrashDumpCallback(const void* gpuCrashDump, const Uint32 gpuCrashDumpSize, void* userData)
{
    GPUCrashTrackerInstance* gpuCrashTracker = reinterpret_cast<GPUCrashTrackerInstance*>(userData);
    gpuCrashTracker->GPUCrashDump(gpuCrashDump, gpuCrashDumpSize);
}

static void ShaderDebugInfoCallback(const void* shaderDebugInfo, const Uint32 shaderDebugInfoSize, void* userData)
{
    GPUCrashTrackerInstance* gpuCrashTracker = reinterpret_cast<GPUCrashTrackerInstance*>(userData);
    gpuCrashTracker->OnShaderDebugInfo(shaderDebugInfo, shaderDebugInfoSize);
	SPT_LOG_INFO(GPUCrashTracker, "ShaderDebugInfoCallback");
}

static void CrashDumpDescriptionCallback(PFN_GFSDK_Aftermath_AddGpuCrashDumpDescription addDescription, void* userData)
{
    GPUCrashTrackerInstance* gpuCrashTracker = reinterpret_cast<GPUCrashTrackerInstance*>(userData);
    gpuCrashTracker->CrashDumpDescription(addDescription, userData);
}

static void ResolveMarkerCallback(const void* marker, void* userData, void** resolvedMarkerData, Uint32* markerSize)
{
    GPUCrashTrackerInstance* gpuCrashTracker = reinterpret_cast<GPUCrashTrackerInstance*>(userData);
    gpuCrashTracker->ResolveMarker(marker, userData, resolvedMarkerData, markerSize);
}
#endif // SPT_ENABLE_NSIGHT_AFTERMATH

//////////////////////////////////////////////////////////////////////////////////////////////////
// GPUCrashTracker ===============================================================================

void GPUCrashTracker::EnableGPUCrashDumps()
{
#if SPT_ENABLE_NSIGHT_AFTERMATH
	const GFSDK_Aftermath_Result result = GFSDK_Aftermath_EnableGpuCrashDumps(GFSDK_Aftermath_Version_API,
																			  GFSDK_Aftermath_GpuCrashDumpWatchedApiFlags_Vulkan,
																			  GFSDK_Aftermath_GpuCrashDumpFeatureFlags_Default,
																			  &GpuCrashDumpCallback,
																			  &ShaderDebugInfoCallback,
																			  &CrashDumpDescriptionCallback,
																			  &ResolveMarkerCallback,
																			  &trackerInstnace);

	SPT_CHECK(result == GFSDK_Aftermath_Result_Success);
#endif // SPT_ENABLE_NSIGHT_AFTERMATH
}

void GPUCrashTracker::SaveGPUCrashDump()
{
	SPT_PROFILER_FUNCTION();

#if SPT_ENABLE_NSIGHT_AFTERMATH
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
        SPT_LOG_FATAL(GPUCrashTracker, "Unexpected crash dump status after timeout: {0}", static_cast<Uint32>(status));
    }
#endif // SPT_ENABLE_NSIGHT_AFTERMATH
}

} // spt::vulkan
