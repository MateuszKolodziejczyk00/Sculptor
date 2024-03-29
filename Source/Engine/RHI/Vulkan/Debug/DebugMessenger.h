#pragma once

#include "Vulkan/VulkanCore.h"
#include "SculptorCoreTypes.h"


namespace spt::vulkan
{

class DebugMessenger
{
public:

	static VkDebugUtilsMessengerCreateInfoEXT	CreateDebugMessengerInfo();

	static VkDebugUtilsMessengerEXT				CreateDebugMessenger(VkInstance instance, const VkAllocationCallbacks* allocator);
	static void									DestroyDebugMessenger(VkDebugUtilsMessengerEXT messenger, VkInstance instance, const VkAllocationCallbacks* allocator);

	static void									EnableWarnings(Bool enable);
};

}