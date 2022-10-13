#include "VulkanDeviceCommon.h"
#include "Vulkan/VulkanCore.h"

namespace spt::vulkan
{

lib::DynamicArray<const char*> VulkanDeviceCommon::GetRequiredDeviceExtensions()
{
    return lib::DynamicArray<const char*>
    {
#if VULKAN_VALIDATION
        VK_EXT_DEBUG_MARKER_EXTENSION_NAME,
#endif // VULKAN_VALIDATION
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };
}

}
