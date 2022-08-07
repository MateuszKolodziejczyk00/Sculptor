#include "PhysicalDevice.h"
#include "VulkanDeviceCommon.h"

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
    SPT_CHECK(selecedDevice != devices.cend());

    return *selecedDevice;
}

VkPhysicalDeviceProperties2 PhysicalDevice::GetDeviceProperties(VkPhysicalDevice device)
{
    SPT_PROFILE_FUNCTION();

    VkPhysicalDeviceProperties2 deviceProps{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
    vkGetPhysicalDeviceProperties2(device, &deviceProps);
    return deviceProps;
}

VkPhysicalDeviceFeatures2 PhysicalDevice::GetDeviceFeatures(VkPhysicalDevice device)
{
    SPT_PROFILE_FUNCTION();

    VkPhysicalDeviceFeatures2 deviceFeatures{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };
    vkGetPhysicalDeviceFeatures2(device, &deviceFeatures);
    return deviceFeatures;
}

spt::lib::DynamicArray<VkQueueFamilyProperties2> PhysicalDevice::GetDeviceQueueFamilies(VkPhysicalDevice device)
{
    SPT_PROFILE_FUNCTION();

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

    return deviceProps.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU
        && deviceFeatures.features.geometryShader
        && deviceFeatures.features.samplerAnisotropy
        && deviceFeatures.features.fillModeNonSolid
        && IsDeviceSupportingQueues(device, VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT, surface)
        && IsDeviceSupportingExtensions(device, VulkanDeviceCommon::GetRequiredDeviceExtensions());
}

Bool PhysicalDevice::IsDeviceSupportingExtensions(VkPhysicalDevice device, const lib::DynamicArray<const char*> extensions)
{
   	Uint32 supportedExtensionsCount = 0;
	vkEnumerateDeviceExtensionProperties(device, nullptr, &supportedExtensionsCount, nullptr);
    lib::DynamicArray<VkExtensionProperties> supportedExtensions = lib::DynamicArray<VkExtensionProperties>(supportedExtensionsCount);
	vkEnumerateDeviceExtensionProperties(device, nullptr, &supportedExtensionsCount, supportedExtensions.data());

   	const auto isExtensionSupported = [&supportedExtensions](const char* extension)
	{
		return std::find_if(supportedExtensions.cbegin(), supportedExtensions.cend(),
			[extension](const VkExtensionProperties& SupportedExtension)
            {
                return std::strcmp(extension, SupportedExtension.extensionName);
            }) != supportedExtensions.cend();
	};

    return std::all_of(extensions.cbegin(), extensions.cend(), isExtensionSupported);
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

}
