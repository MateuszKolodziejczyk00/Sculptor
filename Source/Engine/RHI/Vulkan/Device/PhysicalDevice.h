#pragma once

#include "Vulkan/VulkanCore.h"
#include "SculptorCoreTypes.h"


namespace spt::rhi
{
struct Extension;
} // spt::rhi


namespace spt::vulkan
{

struct PhysicalDevice
{
public:

	static VkPhysicalDevice SelectPhysicalDevice(VkInstance instance, VkSurfaceKHR surface);

	static VkPhysicalDeviceProperties2 GetDeviceProperties(VkPhysicalDevice device);

	static VkPhysicalDeviceFeatures2 GetDeviceFeatures(VkPhysicalDevice device);

	static lib::DynamicArray<VkQueueFamilyProperties2> GetDeviceQueueFamilies(VkPhysicalDevice device);

private:

	static Bool IsDeviceSuitable(VkPhysicalDevice device, VkSurfaceKHR surface);

	static Bool IsDeviceSupportingExtensions(VkPhysicalDevice device, lib::Span<const rhi::Extension> extensions);

	static Bool IsDeviceSupportingQueues(VkPhysicalDevice device, VkQueueFlags requiredQueues, VkSurfaceKHR surface);

	static Bool HasSupportedSubgroupSize(VkPhysicalDevice device);
};

}