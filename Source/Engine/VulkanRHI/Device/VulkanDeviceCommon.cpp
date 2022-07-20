#include "VulkanDeviceCommon.h"
#include "Vulkan.h"

namespace spt::vulkan
{

lib::DynamicArray<const char*> VulkanDeviceCommon::GetRequiredDeviceExtensions()
{
    return lib::DynamicArray<const char*>
    {
#if VULKAN_VALIDATION
        VK_EXT_DEBUG_MARKER_EXTENSION_NAME,
#endif // VULKAN_VALIDATION
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_BIND_MEMORY_2_EXTENSION_NAME,
        VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME
    };
}

}
