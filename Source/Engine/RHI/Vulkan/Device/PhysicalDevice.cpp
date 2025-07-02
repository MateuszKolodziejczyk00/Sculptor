#include "PhysicalDevice.h"
#include "VulkanDeviceCommon.h"
#include "RHICore/RHIPlugin.h"

namespace spt::vulkan
{

VkPhysicalDevice PhysicalDevice::SelectPhysicalDevice(VkInstance instance, VkSurfaceKHR surface)
{
	Uint32 devicesNum = 0;
	SPT_VK_CHECK(vkEnumeratePhysicalDevices(instance, &devicesNum, nullptr));
	SPT_CHECK(devicesNum > 0);

	lib::DynamicArray<VkPhysicalDevice> devices(static_cast<SizeType>(devicesNum));
	SPT_VK_CHECK(vkEnumeratePhysicalDevices(instance, &devicesNum, devices.data()));

	const auto selecedDevice = std::find_if(devices.cbegin(), devices.cend(), [surface](VkPhysicalDevice device) { return IsDeviceSuitable(device, surface); });
	SPT_CHECK_MSG(selecedDevice != devices.cend(), "Couldn't find supported GPU\nProbably because you use GPU with wave64, which currently is not supported in few shaders");

	return *selecedDevice;
}

VkPhysicalDeviceProperties2 PhysicalDevice::GetDeviceProperties(VkPhysicalDevice device)
{
	SPT_PROFILER_FUNCTION();

	VkPhysicalDeviceProperties2 deviceProps{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
	vkGetPhysicalDeviceProperties2(device, &deviceProps);
	return deviceProps;
}

VkPhysicalDeviceFeatures2 PhysicalDevice::GetDeviceFeatures(VkPhysicalDevice device)
{
	SPT_PROFILER_FUNCTION();

	VkPhysicalDeviceFeatures2 deviceFeatures{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };
	vkGetPhysicalDeviceFeatures2(device, &deviceFeatures);
	return deviceFeatures;
}

spt::lib::DynamicArray<VkQueueFamilyProperties2> PhysicalDevice::GetDeviceQueueFamilies(VkPhysicalDevice device)
{
	SPT_PROFILER_FUNCTION();

	Uint32 queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties2(device, &queueFamilyCount, nullptr);
	lib::DynamicArray<VkQueueFamilyProperties2> queueFamilies(queueFamilyCount, VkQueueFamilyProperties2{ VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2 });
	vkGetPhysicalDeviceQueueFamilyProperties2(device, &queueFamilyCount, queueFamilies.data());

	return queueFamilies;
}

Bool PhysicalDevice::IsDeviceSuitable(VkPhysicalDevice device, VkSurfaceKHR surface)
{
	const VkPhysicalDeviceProperties2 deviceProps = GetDeviceProperties(device);

	const VkPhysicalDeviceFeatures2 deviceFeatures = GetDeviceFeatures(device);

	const lib::Span<const rhi::Extension> deviceExtensions = rhi::RHIPluginsManager::Get().GetDeviceExtensions();

	return deviceProps.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU
		&& deviceFeatures.features.geometryShader
		&& deviceFeatures.features.samplerAnisotropy
		&& deviceFeatures.features.fillModeNonSolid
		&& HasSupportedSubgroupSize(device)
		&& IsDeviceSupportingQueues(device, VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT, surface)
		&& IsDeviceSupportingExtensions(device, deviceExtensions);
}

Bool PhysicalDevice::IsDeviceSupportingExtensions(VkPhysicalDevice device, lib::Span<const rhi::Extension> extensions)
{
	Uint32 supportedExtensionsCount = 0;
	vkEnumerateDeviceExtensionProperties(device, nullptr, &supportedExtensionsCount, nullptr);
	lib::DynamicArray<VkExtensionProperties> supportedExtensions = lib::DynamicArray<VkExtensionProperties>(supportedExtensionsCount);
	vkEnumerateDeviceExtensionProperties(device, nullptr, &supportedExtensionsCount, supportedExtensions.data());

	const auto isExtensionSupported = [&supportedExtensions](const rhi::Extension& extension)
	{
		const char* extensionName = extension.GetCStr();

		return std::find_if(supportedExtensions.cbegin(), supportedExtensions.cend(),
							[extensionName](const VkExtensionProperties& supportedExtension)
							{
								const Bool isSupported = std::strcmp(extensionName, supportedExtension.extensionName) == 0;
								return isSupported;
							}) != supportedExtensions.cend();
	};

	const Bool isSupportingAllExtensions = std::all_of(extensions.begin(), extensions.end(), isExtensionSupported);
	return isSupportingAllExtensions;
}

Bool PhysicalDevice::IsDeviceSupportingQueues(VkPhysicalDevice device, VkQueueFlags requiredQueues, VkSurfaceKHR surface)
{
	const lib::DynamicArray<VkQueueFamilyProperties2> queueFamilies = GetDeviceQueueFamilies(device);

	VkQueueFlags remainingRequiredQueues = requiredQueues;

	// if surface is null, set it to true, otherwise find queue that supports presenting on this surface
	Bool hasValidSurfaceSupport = surface == VK_NULL_HANDLE;

	for (SizeType idx = 0; idx < queueFamilies.size(); ++idx)
	{
		const VkQueueFamilyProperties2& familyProps = queueFamilies[idx];

		if (!hasValidSurfaceSupport)
		{
			VkBool32 supportsPresentation = VK_FALSE;
			vkGetPhysicalDeviceSurfaceSupportKHR(device, static_cast<Uint32>(idx), surface, &supportsPresentation);
			hasValidSurfaceSupport |= static_cast<Bool>(supportsPresentation);
		}

		remainingRequiredQueues &= ~familyProps.queueFamilyProperties.queueFlags;
	}

	return remainingRequiredQueues == 0 && hasValidSurfaceSupport;
}

Bool PhysicalDevice::HasSupportedSubgroupSize(VkPhysicalDevice device)
{
	VkPhysicalDeviceSubgroupProperties subgroupProps{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES };

	VkPhysicalDeviceProperties2 deviceProps{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
	deviceProps.pNext = &subgroupProps;

	vkGetPhysicalDeviceProperties2(device, &deviceProps);

	// Currently only NVIDIA GPUs are supported because I don't have other GPU to test stuff (and currently some shaders for sure don't work due to some waveops)
	return subgroupProps.subgroupSize == 32u;
}

}
