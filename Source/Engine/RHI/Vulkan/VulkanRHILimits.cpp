#include "VulkanRHILimits.h"

namespace spt::vulkan
{

namespace priv
{

static VkPhysicalDeviceProperties2 g_properties;

static VkPhysicalDeviceRayTracingPipelinePropertiesKHR g_rayTracingProperties;

static VkPhysicalDeviceDescriptorHeapPropertiesEXT g_descriptorHeapProperties;

static const VkPhysicalDeviceLimits& GetDeviceLimits()
{
	return g_properties.properties.limits;
}

} // priv

void VulkanRHILimits::Initialize(const LogicalDevice& logicalDevice, VkPhysicalDevice physicalDevice)
{
	priv::g_rayTracingProperties = VkPhysicalDeviceRayTracingPipelinePropertiesKHR{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR };
	priv::g_properties = VkPhysicalDeviceProperties2{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
	priv::g_descriptorHeapProperties = VkPhysicalDeviceDescriptorHeapPropertiesEXT{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_HEAP_PROPERTIES_EXT };

	priv::g_properties.pNext = &priv::g_rayTracingProperties;
	priv::g_rayTracingProperties.pNext = &priv::g_descriptorHeapProperties;

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

const VkPhysicalDeviceDescriptorHeapPropertiesEXT& VulkanRHILimits::GetDescriptorProps()
{
	return priv::g_descriptorHeapProperties;
}

} // spt::vulkan
