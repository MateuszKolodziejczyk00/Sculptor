#include "VulkanRHI.h"
#include "Device/PhysicalDevice.h"
#include "Device/LogicalDevice.h"
#include "VulkanRHILimits.h"
#include "Debug/DebugMessenger.h"
#include "Memory/MemoryManager.h"
#include "CommandPool/RHICommandPoolsManager.h"
#include "VulkanTypes/RHISemaphore.h"
#include "VulkanTypes/RHICommandBuffer.h"
#include "VulkanUtils.h"
#include "LayoutsManager.h"
#include "Pipeline/PipelineLayoutsManager.h"

#include "RHICore/RHIInitialization.h"
#include "RHICore/RHISubmitTypes.h"

#include "Logging/Log.h"


namespace spt::vulkan
{

SPT_IMPLEMENT_LOG_CATEGORY(VulkanRHI, true);

namespace priv
{

// VulkanInstanceData ============================================================================

class VulkanInstanceData
{
public:

    VulkanInstanceData()
        : instance(VK_NULL_HANDLE)
        , physicalDevice(VK_NULL_HANDLE)
        , surface(VK_NULL_HANDLE)
        , debugMessenger(VK_NULL_HANDLE)
    { }

    VkInstance                  instance;

    VkPhysicalDevice            physicalDevice;
    LogicalDevice               device;

    MemoryManager               memoryManager;

    VkSurfaceKHR                surface;
    
    VkDebugUtilsMessengerEXT    debugMessenger;

    CommandPoolsManager         commandPoolsManager;

    LayoutsManager              layoutsManager;

    PipelineLayoutsManager      pipelineLayoutsManager;
};

static VulkanInstanceData g_data;

// Volk ==========================================================================================

void InitializeVolk()
{
    volkInitialize();
}

void VolkLoadInstance(VkInstance instance)
{
    volkLoadInstance(instance);
}

} // priv

// Vulkan RHI ====================================================================================

void VulkanRHI::Initialize(const rhi::RHIInitializationInfo& initInfo)
{
    priv::InitializeVolk();

    VkApplicationInfo applicationInfo{ VK_STRUCTURE_TYPE_APPLICATION_INFO };
    applicationInfo.pApplicationName = "Sculptor";
    applicationInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    applicationInfo.pEngineName = "Sculptor";
    applicationInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    applicationInfo.apiVersion = VK_API_VERSION_1_3;

    VkInstanceCreateInfo instanceInfo{ VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
    instanceInfo.flags = 0;
    instanceInfo.pApplicationInfo= &applicationInfo;

    // Extensions
    const auto getExtensionNamePtr = [](std::string_view extensionName)
    {
        return extensionName.data();
    };

    lib::DynamicArray<const char*> extensionNames;
    extensionNames.reserve(initInfo.extensionsNum);
    
    for (Uint32 i = 0; i < initInfo.extensionsNum; ++i)
    {
        extensionNames.push_back(initInfo.extensions[i]);
    }
    
#if VULKAN_VALIDATION

    extensionNames.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
    extensionNames.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

#endif // VULKAN_VALIDATION

    instanceInfo.enabledExtensionCount = static_cast<Uint32>(extensionNames.size());
    instanceInfo.ppEnabledExtensionNames = !extensionNames.empty() ? extensionNames.data() : VK_NULL_HANDLE;

#if VULKAN_VALIDATION
    const char* enabledLayers[] =
    {
        VULKAN_VALIDATION_LAYER_NAME
    };

    instanceInfo.enabledLayerCount = SPT_ARRAY_SIZE(enabledLayers);
    instanceInfo.ppEnabledLayerNames = enabledLayers;
#endif // VULKAN_VALIDATION

    VulkanStructsLinkedList instanceInfoLinkedList(instanceInfo);

#if VULKAN_VALIDATION

    VkDebugUtilsMessengerCreateInfoEXT debugMessengerInfo = DebugMessenger::CreateDebugMessengerInfo();
    instanceInfoLinkedList.Append(debugMessengerInfo);

#endif // VULKAN_VALIDATION

#if VULKAN_VALIDATION_STRICT

    static_assert(VULKAN_VALIDATION);

    lib::DynamicArray<VkValidationFeatureEnableEXT> enabledValidationFeatures;

#if VULKAN_VALIDATION_STRICT_GPU_ASSISTED
    enabledValidationFeatures.push_back(VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_EXT);
    enabledValidationFeatures.push_back(VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_RESERVE_BINDING_SLOT_EXT);
#endif // VULKAN_VALIDATION_STRICT_GPU_ASSISTED

#if VULKAN_VALIDATION_STRICT_BEST_PRACTICES
    enabledValidationFeatures.push_back(VK_VALIDATION_FEATURE_ENABLE_BEST_PRACTICES_EXT);
#endif // VULKAN_VALIDATION_STRICT_BEST_PRACTICES

#if VULKAN_VALIDATION_STRICT_DEBUG_PRINTF
    static_assert(!VULKAN_VALIDATION_STRICT_GPU_ASSISTED);
    enabledValidationFeatures.push_back(VK_VALIDATION_FEATURE_ENABLE_DEBUG_PRINTF_EXT);
#endif // VULKAN_VALIDATION_STRICT_DEBUG_PRINTF

#if VULKAN_VALIDATION_STRICT_SYNCHRONIZATION
    enabledValidationFeatures.push_back(VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT);
#endif // VULKAN_VALIDATION_STRICT_SYNCHRONIZATION

    VkValidationFeaturesEXT validationFeatures{ VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT };
    validationFeatures.enabledValidationFeatureCount = static_cast<Uint32>(enabledValidationFeatures.size());
    validationFeatures.pEnabledValidationFeatures = enabledValidationFeatures.data();

    instanceInfoLinkedList.Append(validationFeatures);

#endif // VULKAN_VALIDATION_STRICT

    SPT_VK_CHECK(vkCreateInstance(&instanceInfo, GetAllocationCallbacks(), &priv::g_data.instance));

    priv::VolkLoadInstance(priv::g_data.instance);

#if VULKAN_VALIDATION
    priv::g_data.debugMessenger = DebugMessenger::CreateDebugMessenger(priv::g_data.instance, GetAllocationCallbacks());
#endif // VULKAN_VALIDATION

    priv::g_data.pipelineLayoutsManager.InitializeRHI();
}

void VulkanRHI::InitializeGPUForWindow()
{
    SPT_CHECK(!!priv::g_data.instance);
    SPT_CHECK(!!priv::g_data.surface);

    priv::g_data.physicalDevice = PhysicalDevice::SelectPhysicalDevice(priv::g_data.instance, priv::g_data.surface);

    if (SPT_IS_LOG_CATEGORY_ENABLED(VulkanRHI))
    {
        const VkPhysicalDeviceProperties2 deviceProps = PhysicalDevice::GetDeviceProperties(priv::g_data.physicalDevice);
        SPT_LOG_TRACE(VulkanRHI, "Selected Device: {0}", deviceProps.properties.deviceName);
    }

    priv::g_data.device.CreateDevice(priv::g_data.physicalDevice, GetAllocationCallbacks());

    VulkanRHILimits::Initialize(GetLogicalDevice(), GetPhysicalDeviceHandle());

    priv::g_data.memoryManager.Initialize(priv::g_data.instance, priv::g_data.device.GetHandle(), priv::g_data.physicalDevice, GetAllocationCallbacks());
}

void VulkanRHI::Uninitialize()
{
    priv::g_data.commandPoolsManager.DestroyResources();

    priv::g_data.pipelineLayoutsManager.ReleaseRHI();

    if (priv::g_data.surface)
    {
        vkDestroySurfaceKHR(priv::g_data.instance, priv::g_data.surface, GetAllocationCallbacks());
        priv::g_data.surface = VK_NULL_HANDLE;
    }

    if (priv::g_data.memoryManager.IsValid())
    {
        priv::g_data.memoryManager.Destroy();
    }

    if (priv::g_data.device.IsValid())
    {
        priv::g_data.device.Destroy(GetAllocationCallbacks());
    }

    if (priv::g_data.debugMessenger)
    {
        DebugMessenger::DestroyDebugMessenger(priv::g_data.debugMessenger, priv::g_data.instance, GetAllocationCallbacks());
        priv::g_data.debugMessenger = VK_NULL_HANDLE;
    }

    if (priv::g_data.instance)
    {
        vkDestroyInstance(priv::g_data.instance, GetAllocationCallbacks());
        priv::g_data.instance = VK_NULL_HANDLE;
    }
}

void VulkanRHI::BeginFrame()
{
    priv::g_data.pipelineLayoutsManager.FlushPendingPipelineLayouts();
}

void VulkanRHI::EndFrame()
{

}

rhi::ERHIType VulkanRHI::GetRHIType()
{
    return rhi::ERHIType::Vulkan_1_3;
}

void VulkanRHI::SubmitCommands(rhi::ECommandBufferQueueType queueType, const lib::DynamicArray<rhi::SubmitBatchData>& submitBatches)
{
    SPT_PROFILER_FUNCTION();

    SPT_CHECK(!submitBatches.empty());

    lib::DynamicArray<VkSubmitInfo2> submitInfos;
    submitInfos.resize(submitBatches.size());

    lib::DynamicArray<lib::DynamicArray<VkCommandBufferSubmitInfo>> commandBuffersBatches;
    commandBuffersBatches.resize(submitBatches.size());

    for (SizeType idx = 0; idx < submitBatches.size(); ++idx)
    {
        const RHISemaphoresArray* waitSemaphores                    = submitBatches[idx].waitSemaphores;
        const RHISemaphoresArray* signalSemaphores                  = submitBatches[idx].signalSemaphores;
        const lib::DynamicArray<const RHICommandBuffer*> cmdBuffers = submitBatches[idx].commandBuffers;

        SPT_CHECK(!cmdBuffers.empty());

        lib::DynamicArray<VkCommandBufferSubmitInfo>& cmdBuffersSubmitBatch = commandBuffersBatches[idx];
        cmdBuffersSubmitBatch.resize(cmdBuffers.size());

        for (SizeType batchIdx = 0; batchIdx < cmdBuffers.size(); ++batchIdx)
        {
            SPT_CHECK(cmdBuffers[batchIdx]->GetQueueType() == queueType);

            VkCommandBufferSubmitInfo& cmdBufferSubmit  = cmdBuffersSubmitBatch[batchIdx];
            cmdBufferSubmit.sType                       = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
            cmdBufferSubmit.commandBuffer               = cmdBuffers[batchIdx]->GetHandle();
            cmdBufferSubmit.deviceMask                  = 0;
        }

        VkSubmitInfo2& submitInfo = submitInfos[idx];
        submitInfo.sType                    = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
        submitInfo.flags                    = 0;
        submitInfo.waitSemaphoreInfoCount   = waitSemaphores ? static_cast<Uint32>(waitSemaphores->GetSemaphoresNum()) : 0;
        submitInfo.pWaitSemaphoreInfos      = waitSemaphores ? waitSemaphores->GetSubmitInfos().data() : nullptr;
        submitInfo.commandBufferInfoCount   = static_cast<Uint32>(cmdBuffersSubmitBatch.size());
        submitInfo.pCommandBufferInfos      = cmdBuffersSubmitBatch.data();
        submitInfo.signalSemaphoreInfoCount = signalSemaphores ? static_cast<Uint32>(signalSemaphores->GetSemaphoresNum()) : 0;;
        submitInfo.pSignalSemaphoreInfos    = signalSemaphores ? signalSemaphores->GetSubmitInfos().data() : nullptr;
    }

    SPT_VK_CHECK(vkQueueSubmit2(GetLogicalDevice().GetQueueHandle(queueType), static_cast<Uint32>(submitInfos.size()), submitInfos.data(), VK_NULL_HANDLE));
}

void VulkanRHI::WaitIdle()
{
    priv::g_data.device.WaitIdle();
}

void VulkanRHI::EnableValidationWarnings(Bool enable)
{
    DebugMessenger::EnableWarnings(enable);
}

VkInstance VulkanRHI::GetInstanceHandle()
{
    return priv::g_data.instance;
}

VkDevice VulkanRHI::GetDeviceHandle()
{
    return priv::g_data.device.GetHandle();
}

VkPhysicalDevice VulkanRHI::GetPhysicalDeviceHandle()
{
    return priv::g_data.physicalDevice;
}

CommandPoolsManager& VulkanRHI::GetCommandPoolsManager()
{
    return priv::g_data.commandPoolsManager;
}

LayoutsManager& VulkanRHI::GetLayoutsManager()
{
    return priv::g_data.layoutsManager;
}

PipelineLayoutsManager& VulkanRHI::GetPipelineLayoutsManager()
{
    return priv::g_data.pipelineLayoutsManager;
}

VkSurfaceKHR VulkanRHI::GetSurfaceHandle()
{
    return priv::g_data.surface;
}

const LogicalDevice& VulkanRHI::GetLogicalDevice()
{
    return priv::g_data.device;
}

MemoryManager& VulkanRHI::GetMemoryManager()
{
    return priv::g_data.memoryManager;
}

VmaAllocator VulkanRHI::GetAllocatorHandle()
{
    return priv::g_data.memoryManager.GetAllocatorHandle();
}

void VulkanRHI::SetSurfaceHandle(VkSurfaceKHR surface)
{
    priv::g_data.surface = surface;
}

const VkAllocationCallbacks* VulkanRHI::GetAllocationCallbacks()
{
    return nullptr;
}

} // spt::vulkan
