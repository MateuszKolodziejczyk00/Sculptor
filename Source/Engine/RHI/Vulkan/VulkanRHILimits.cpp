#include "VulkanRHILimits.h"

namespace spt::vulkan
{

namespace priv
{

static VkPhysicalDeviceProperties2 g_properties;

static VkPhysicalDeviceRayTracingPipelinePropertiesKHR g_rayTracingProperties;

static const VkPhysicalDeviceLimits& GetDeviceLimits()
{
	return g_properties.properties.limits;
}

} // priv

void VulkanRHILimits::Initialize(const LogicalDevice& logicalDevice, VkPhysicalDevice physicalDevice)
{
	priv::g_rayTracingProperties = VkPhysicalDeviceRayTracingPipelinePropertiesKHR{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR };
	priv::g_properties = VkPhysicalDeviceProperties2{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
	priv::g_properties.pNext = &priv::g_rayTracingProperties;
	vkGetPhysicalDeviceProperties2(physicalDevice, &priv::g_properties);
}

Uint64 VulkanRHILimits::GetMinUniformBufferOffsetAlignment()
{
	return priv::GetDeviceLimits().minUniformBufferOffsetAlignment;
}

Uint64 VulkanRHILimits::GetOptimalBufferCopyOffsetAlignment()
{
	return priv::GetDeviceLimits().optimalBufferCopyOffsetAlignment;
}

const VkPhysicalDeviceProperties2& VulkanRHILimits::GetProperties()
{
	return priv::g_properties;
}

const VkPhysicalDeviceRayTracingPipelinePropertiesKHR& VulkanRHILimits::GetRayTracingPipelineProperties()
{
	return priv::g_rayTracingProperties;
}

} // spt::vulkan
