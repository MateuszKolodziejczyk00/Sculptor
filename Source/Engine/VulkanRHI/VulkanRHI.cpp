#include "VulkanRHI.h"
#include "Logging/Log.h"


namespace spt::vulkan
{

SPT_IMPLEMENT_LOG_CATEGORY(VulkanRHI, true);

namespace priv
{

void InitializeVolk()
{
    volkInitialize();
}

class VulkanInstanceData
{
public:

    VulkanInstanceData()
        : m_instance(VK_NULL_HANDLE)
    { }

    VkInstance m_instance;
};

VulkanInstanceData g_vulkanInstance;

}

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
    
    for (uint32 i = 0; i < initInfo.m_extensionsNum; ++i)
    {
        extensionNames.push_back(initInfo.m_extensions[i]);
    }
    
    instanceInfo.enabledExtensionCount = static_cast<uint32>(extensionNames.size());
    instanceInfo.ppEnabledExtensionNames = !extensionNames.empty() ? extensionNames.data() : VK_NULL_HANDLE;

    const char* enabledLayers[] = {
#if VULKAN_VALIDATION
        VULKAN_VALIDATION_LAYER_NAME
#endif // VULKAN_VALIDATION
    };

    instanceInfo.enabledLayerCount = SPT_ARRAY_SIZE(enabledLayers);
    instanceInfo.ppEnabledLayerNames = enabledLayers;

    SPT_VK_CHECK(vkCreateInstance(&instanceInfo, GetAllocationCallbacks(), &priv::g_vulkanInstance.m_instance));
}

void VulkanRHI::Uninitialize()
{
    if (priv::g_vulkanInstance.m_instance)
    {
        vkDestroyInstance(priv::g_vulkanInstance.m_instance, GetAllocationCallbacks());
        priv::g_vulkanInstance.m_instance = VK_NULL_HANDLE;
    }
}

VkAllocationCallbacks* VulkanRHI::GetAllocationCallbacks()
{
    return nullptr;
}

}
