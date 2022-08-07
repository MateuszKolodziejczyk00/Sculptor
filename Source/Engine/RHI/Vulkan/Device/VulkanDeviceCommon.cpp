#include "VulkanDeviceCommon.h"
#include "Vulkan/VulkanCore.h"

namespace spt::vulkan
{

lib::DynamicArray<const char*> VulkanDeviceCommon::GetRequiredDeviceExtensions()
{
    return lib::DynamicArray<const char*>
    {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };
}

}
