#include "VulkanDeviceCommon.h"
#include "Vulkan/VulkanCore.h"
#include "Vulkan/VulkanRHI.h"

namespace spt::vulkan
{

lib::DynamicArray<const char*> VulkanDeviceCommon::GetRequiredDeviceExtensions()
{
    lib::DynamicArray<const char*> requiredExtensions;

    requiredExtensions.emplace_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

#if RHI_DEBUG
    requiredExtensions.emplace_back(VK_EXT_DEBUG_MARKER_EXTENSION_NAME);
#endif // RHI_DEBUG

#if WITH_NSIGHT_AFTERMATH

    if (VulkanRHI::GetSettings().AreGPUCrashDumpsEnabled())
    {
        requiredExtensions.emplace_back(VK_NV_DEVICE_DIAGNOSTIC_CHECKPOINTS_EXTENSION_NAME);
        requiredExtensions.emplace_back(VK_NV_DEVICE_DIAGNOSTICS_CONFIG_EXTENSION_NAME);
    }

#endif // WITH_NSIGHT_AFTERMATH

    if (VulkanRHI::GetSettings().IsRayTracingEnabled())
    {
        requiredExtensions.emplace_back(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
        requiredExtensions.emplace_back(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME);
        requiredExtensions.emplace_back(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
        requiredExtensions.emplace_back(VK_KHR_RAY_QUERY_EXTENSION_NAME);
    }

    return requiredExtensions;
}

} // spt::vulkan
