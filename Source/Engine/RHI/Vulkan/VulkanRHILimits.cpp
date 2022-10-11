#include "VulkanRHILimits.h"

namespace spt::vulkan
{

namespace priv
{

static VkPhysicalDeviceProperties2 g_properties;

static const VkPhysicalDeviceLimits& GetDeviceLimits()
{
	return g_properties.properties.limits;
}

} // priv

void VulkanRHILimits::Initialize(const LogicalDevice& logicalDevice, VkPhysicalDevice physicalDevice)
{
	vkGetPhysicalDeviceProperties2(physicalDevice, &priv::g_properties);
}

Uint64 VulkanRHILimits::GetMinUniformBufferOffsetAlignment()
{
	return priv::GetDeviceLimits().minUniformBufferOffsetAlignment;
}

} // spt::vulkan
