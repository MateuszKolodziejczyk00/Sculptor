#include "VulkanRHI.h"
#include "Debug/DebugMessenger.h"

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

    VkInstance m_instance;

    VkPhysicalDevice m_physicalDevice;

    VkSurfaceKHR m_surface;
    
    VkDebugUtilsMessengerEXT m_debugMessenger;
};

VulkanInstanceData g_vulkanInstance;

// Volk ==========================================================================================

void InitializeVolk()
{
    volkInitialize();
}

void VolkLoadInstance(VkInstance instance)
{
    volkLoadInstance(instance);
}

// Utils =========================================================================================

lib::DynamicArray<const char*> GetRequiredDeviceExtensions()
{
    return lib::DynamicArray<const char*>{
#if VULKAN_VALIDATION
        VK_EXT_DEBUG_MARKER_EXTENSION_NAME,
#endif // VULKAN_VALIDATION
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_BIND_MEMORY_2_EXTENSION_NAME,
        VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME
    };
}

// Physical Device ===============================================================================

Bool IsDeviceSupportingQueues(VkPhysicalDevice device, VkQueueFlags requiredQueues, VkSurfaceKHR surface)
{
   	Uint32 queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
	lib::DynamicArray<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

    VkQueueFlags remainingRequiredQueues = requiredQueues;

    // if surface is null, set it to true, otherwise find queue that supports presenting on this surface
    Bool hasValidSurfaceSupport = surface == VK_NULL_HANDLE;

    for (SizeType idx = 0; idx < queueFamilies.size(); ++idx)
    {
        const VkQueueFamilyProperties& familyProps = queueFamilies[idx];

        if (!hasValidSurfaceSupport)
        {
            VkBool32 supportsPresentation = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, static_cast<Uint32>(idx), surface, &supportsPresentation);
            hasValidSurfaceSupport |= static_cast<Bool>(supportsPresentation);
        }

        remainingRequiredQueues &= ~familyProps.queueFlags;
    }

    return remainingRequiredQueues == 0 && hasValidSurfaceSupport;
}

Bool IsDeviceSupportingExtensions(VkPhysicalDevice device, const lib::DynamicArray<const char*>& extensions)
{
   	Uint32 supportedExtensionsCount = 0;
	vkEnumerateDeviceExtensionProperties(device, nullptr, &supportedExtensionsCount, nullptr);
	lib::DynamicArray<VkExtensionProperties> supportedExtensions(supportedExtensionsCount);
	vkEnumerateDeviceExtensionProperties(device, nullptr, &supportedExtensionsCount, supportedExtensions.data());

   	const auto isExtensionSupported = [&supportedExtensions](const char* extension)
	{
		return std::find_if(supportedExtensions.cbegin(), supportedExtensions.cend(),
			[extension](const VkExtensionProperties& SupportedExtension)
            {
                return std::strcmp(extension, SupportedExtension.extensionName);
            }) != supportedExtensions.cend();
	};

    return std::all_of(extensions.cbegin(), extensions.cend(), isExtensionSupported);
}

Bool IsDeviceSuitable(VkPhysicalDevice device)
{
    VkPhysicalDeviceProperties2 deviceProps{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
    vkGetPhysicalDeviceProperties2(device, &deviceProps);

    VkPhysicalDeviceFeatures2 deviceFeatures{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };
    vkGetPhysicalDeviceFeatures2(device, &deviceFeatures);

    return deviceProps.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU
        && deviceFeatures.features.geometryShader
        && deviceFeatures.features.samplerAnisotropy
        && deviceFeatures.features.fillModeNonSolid
        && IsDeviceSupportingQueues(device, VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT, g_vulkanInstance.m_surface)
        && IsDeviceSupportingExtensions(device, GetRequiredDeviceExtensions());
}

VkPhysicalDevice PickPhysicalDevice(VkInstance instance)
{
    Uint32 devicesNum = 0;
    vkEnumeratePhysicalDevices(instance, &devicesNum, nullptr);
    SPT_CHECK(devicesNum > 0);

    lib::DynamicArray<VkPhysicalDevice> devices(static_cast<SizeType>(devicesNum));
    vkEnumeratePhysicalDevices(instance, &devicesNum, devices.data());

    const auto selecedDevice = std::find_if(devices.cbegin(), devices.cend(), [](VkPhysicalDevice device) { return IsDeviceSuitable(device); });
    SPT_CHECK(selecedDevice != devices.cend());

    return *selecedDevice;
}

} // priv

// Vulkan RHI ====================================================================================

void VulkanRHI::Initialize(const rhicore::RHIInitializationInfo& initInfo)
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

    const char* enabledLayers[] = {
#if VULKAN_VALIDATION
        VULKAN_VALIDATION_LAYER_NAME
#endif // VULKAN_VALIDATION
    };

    instanceInfo.enabledLayerCount = SPT_ARRAY_SIZE(enabledLayers);
    instanceInfo.ppEnabledLayerNames = enabledLayers;

#if VULKAN_VALIDATION

    const VkDebugUtilsMessengerCreateInfoEXT debugMessengerInfo = DebugMessenger::CreateDebugMessengerInfo();
    instanceInfo.pNext = &debugMessengerInfo;

#endif // VULKAN_VALIDATION

    SPT_VK_CHECK(vkCreateInstance(&instanceInfo, GetAllocationCallbacks(), &priv::g_vulkanInstance.m_instance));

    priv::VolkLoadInstance(priv::g_vulkanInstance.m_instance);

    priv::g_vulkanInstance.m_debugMessenger = DebugMessenger::CreateDebugMessenger(priv::g_vulkanInstance.m_instance, GetAllocationCallbacks());
}

void VulkanRHI::SelectAndInitializeGPU()
{
    SPT_CHECK(!!priv::g_vulkanInstance.m_instance);
    SPT_CHECK(!!priv::g_vulkanInstance.m_surface);

    priv::g_vulkanInstance.m_physicalDevice = priv::PickPhysicalDevice(priv::g_vulkanInstance.m_instance);

    if (SPT_IS_LOG_CATEGORY_ENABLED(VulkanRHI))
    {
        VkPhysicalDeviceProperties deviceProps{};
        vkGetPhysicalDeviceProperties(priv::g_vulkanInstance.m_physicalDevice, &deviceProps);
        SPT_LOG_TRACE(VulkanRHI, "Selected Device: {0}", deviceProps.deviceName);
    }

}


void VulkanRHI::Uninitialize()
{
    if (priv::g_vulkanInstance.m_debugMessenger)
    {
        DebugMessenger::DestroyDebugMessenger(priv::g_vulkanInstance.m_debugMessenger, priv::g_vulkanInstance.m_instance, GetAllocationCallbacks());
        priv::g_vulkanInstance.m_debugMessenger = VK_NULL_HANDLE;
    }

    if (priv::g_vulkanInstance.m_surface)
    {
        vkDestroySurfaceKHR(priv::g_vulkanInstance.m_instance, priv::g_vulkanInstance.m_surface, GetAllocationCallbacks());
        priv::g_vulkanInstance.m_surface = VK_NULL_HANDLE;
    }

    if (priv::g_vulkanInstance.m_instance)
    {
        vkDestroyInstance(priv::g_vulkanInstance.m_instance, GetAllocationCallbacks());
        priv::g_vulkanInstance.m_instance = VK_NULL_HANDLE;
    }
}

VkInstance VulkanRHI::GetVulkanInstance()
{
    return priv::g_vulkanInstance.m_instance;
}

void VulkanRHI::SetVulkanSurface(VkSurfaceKHR surface)
{
    priv::g_vulkanInstance.m_surface = surface;
}

const VkAllocationCallbacks* VulkanRHI::GetAllocationCallbacks()
{
    return nullptr;
}

}
