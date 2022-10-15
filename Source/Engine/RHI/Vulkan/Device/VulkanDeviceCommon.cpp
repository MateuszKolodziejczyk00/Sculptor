#include "VulkanDeviceCommon.h"
#include "Vulkan/VulkanCore.h"

namespace spt::vulkan
{

lib::DynamicArray<const char*> VulkanDeviceCommon::GetRequiredDeviceExtensions()
{
    return lib::DynamicArray<const char*>
    {
#if RHI_DEBUG
        VK_EXT_DEBUG_MARKER_EXTENSION_NAME,
#endif // RHI_DEBUG
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };
}

}
