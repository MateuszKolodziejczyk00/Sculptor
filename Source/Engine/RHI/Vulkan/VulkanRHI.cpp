#include "VulkanRHI.h"
#include "Device/PhysicalDevice.h"
#include "Device/LogicalDevice.h"
#include "Debug/DebugMessenger.h"
#include "Memory/MemoryManager.h"
#include "CommandPool/RHICommandPoolsManager.h"
#include "VulkanTypes/RHISemaphore.h"
#include "VulkanTypes/RHICommandBuffer.h"
#include "VulkanUtils.h"
#include "LayoutsManager.h"

#include "RHICore/RHIInitialization.h"
#include "RHICore/RHISubmitTypes.h"

#include "Utility/HashedString.h"
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
        : m_instance(VK_NULL_HANDLE)
        , m_physicalDevice(VK_NULL_HANDLE)
        , m_surface(VK_NULL_HANDLE)
        , m_debugMessenger(VK_NULL_HANDLE)
    { }

    VkInstance                  m_instance;

    VkPhysicalDevice            m_physicalDevice;
    LogicalDevice               m_device;

    MemoryManager               m_memoryManager;

    VkSurfaceKHR                m_surface;
    
    VkDebugUtilsMessengerEXT    m_debugMessenger;

    CommandPoolsManager      m_commandPoolsManager;

    LayoutsManager              m_layoutsManager;
};

VulkanInstanceData g_data;

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
    extensionNames.reserve(initInfo.m_extensionsNum);
    
    for (Uint32 i = 0; i < initInfo.m_extensionsNum; ++i)
    {
        extensionNames.push_back(initInfo.m_extensions[i]);
    }
    
#if VULKAN_VALIDATION

    extensionNames.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

#endif // VULKAN_VALIDATION

    instanceInfo.enabledExtensionCount = static_cast<Uint32>(extensionNames.size());
    instanceInfo.ppEnabledExtensionNames = !extensionNames.empty() ? extensionNames.data() : VK_NULL_HANDLE;

    const char* enabledLayers[] =
    {
#if VULKAN_VALIDATION
        VULKAN_VALIDATION_LAYER_NAME
#endif // VULKAN_VALIDATION
    };

    instanceInfo.enabledLayerCount = SPT_ARRAY_SIZE(enabledLayers);
    instanceInfo.ppEnabledLayerNames = enabledLayers;

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

    SPT_VK_CHECK(vkCreateInstance(&instanceInfo, GetAllocationCallbacks(), &priv::g_data.m_instance));

    priv::VolkLoadInstance(priv::g_data.m_instance);

    priv::g_data.m_debugMessenger = DebugMessenger::CreateDebugMessenger(priv::g_data.m_instance, GetAllocationCallbacks());
}

void VulkanRHI::InitializeGPUForWindow()
{
    SPT_CHECK(!!priv::g_data.m_instance);
    SPT_CHECK(!!priv::g_data.m_surface);

    priv::g_data.m_physicalDevice = PhysicalDevice::SelectPhysicalDevice(priv::g_data.m_instance, priv::g_data.m_surface);

    if (SPT_IS_LOG_CATEGORY_ENABLED(VulkanRHI))
    {
        const VkPhysicalDeviceProperties2 deviceProps = PhysicalDevice::GetDeviceProperties(priv::g_data.m_physicalDevice);
        SPT_LOG_TRACE(VulkanRHI, "Selected Device: {0}", deviceProps.properties.deviceName);
    }

    priv::g_data.m_device.CreateDevice(priv::g_data.m_physicalDevice, GetAllocationCallbacks());

    priv::g_data.m_memoryManager.Initialize(priv::g_data.m_instance, priv::g_data.m_device.GetHandle(), priv::g_data.m_physicalDevice, GetAllocationCallbacks());
}

void VulkanRHI::Uninitialize()
{
    priv::g_data.m_commandPoolsManager.DestroyResources();

    if (priv::g_data.m_surface)
    {
        vkDestroySurfaceKHR(priv::g_data.m_instance, priv::g_data.m_surface, GetAllocationCallbacks());
        priv::g_data.m_surface = VK_NULL_HANDLE;
    }

    if (priv::g_data.m_memoryManager.IsValid())
    {
        priv::g_data.m_memoryManager.Destroy();
    }

    if (priv::g_data.m_device.IsValid())
    {
        priv::g_data.m_device.Destroy(GetAllocationCallbacks());
    }

    if (priv::g_data.m_debugMessenger)
    {
        DebugMessenger::DestroyDebugMessenger(priv::g_data.m_debugMessenger, priv::g_data.m_instance, GetAllocationCallbacks());
        priv::g_data.m_debugMessenger = VK_NULL_HANDLE;
    }

    if (priv::g_data.m_instance)
    {
        vkDestroyInstance(priv::g_data.m_instance, GetAllocationCallbacks());
        priv::g_data.m_instance = VK_NULL_HANDLE;
    }
}

void VulkanRHI::SubmitCommands(rhi::ECommandBufferQueueType queueType, const lib::DynamicArray<rhi::SubmitBatchData>& submitBatches)
{
    SPT_PROFILE_FUNCTION();

    SPT_CHECK(!submitBatches.empty());

    lib::DynamicArray<VkSubmitInfo2> submitInfos;
    submitInfos.resize(submitBatches.size());

    lib::DynamicArray<lib::DynamicArray<VkCommandBufferSubmitInfo>> commandBuffersBatches;
    commandBuffersBatches.resize(submitBatches.size());

    for (SizeType idx = 0; idx < submitBatches.size(); ++idx)
    {
        const RHISemaphoresArray* waitSemaphores                    = submitBatches[idx].m_waitSemaphores;
        const RHISemaphoresArray* signalSemaphores                  = submitBatches[idx].m_signalSemaphores;
        const lib::DynamicArray<const RHICommandBuffer*> cmdBuffers = submitBatches[idx].m_commandBuffers;

        SPT_CHECK(!cmdBuffers.empty());

        lib::DynamicArray<VkCommandBufferSubmitInfo>& cmdBuffersSubmitBatch = commandBuffersBatches[idx];
        cmdBuffersSubmitBatch.resize(cmdBuffers.size());

        for (SizeType idx = 0; idx < cmdBuffers.size(); ++idx)
        {
            SPT_CHECK(cmdBuffers[idx]->GetQueueType() == queueType);

            VkCommandBufferSubmitInfo& cmdBufferSubmit  = cmdBuffersSubmitBatch[idx];
            cmdBufferSubmit.sType                       = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
            cmdBufferSubmit.commandBuffer               = cmdBuffers[idx]->GetHandle();
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
    priv::g_data.m_device.WaitIdle();
}

VkInstance VulkanRHI::GetInstanceHandle()
{
    return priv::g_data.m_instance;
}

VkDevice VulkanRHI::GetDeviceHandle()
{
    return priv::g_data.m_device.GetHandle();
}

VkPhysicalDevice VulkanRHI::GetPhysicalDeviceHandle()
{
    return priv::g_data.m_physicalDevice;
}

CommandPoolsManager& VulkanRHI::GetCommandPoolsManager()
{
    return priv::g_data.m_commandPoolsManager;
}

LayoutsManager& VulkanRHI::GetLayoutsManager()
{
    return priv::g_data.m_layoutsManager;
}

VkSurfaceKHR VulkanRHI::GetSurfaceHandle()
{
    return priv::g_data.m_surface;
}

const LogicalDevice& VulkanRHI::GetLogicalDevice()
{
    return priv::g_data.m_device;
}

MemoryManager& VulkanRHI::GetMemoryManager()
{
    return priv::g_data.m_memoryManager;
}

VmaAllocator VulkanRHI::GetAllocatorHandle()
{
    return priv::g_data.m_memoryManager.GetAllocatorHandle();
}

void VulkanRHI::SetSurfaceHandle(VkSurfaceKHR surface)
{
    priv::g_data.m_surface = surface;
}

const VkAllocationCallbacks* VulkanRHI::GetAllocationCallbacks()
{
    return nullptr;
}

}