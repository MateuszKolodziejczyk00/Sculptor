#pragma once

#include "Vulkan.h"

namespace spt::vulkan
{

class DebugMessenger
{
public:

	static VkDebugUtilsMessengerCreateInfoEXT	CreateDebugMessengerInfo();

	static VkDebugUtilsMessengerEXT				CreateDebugMessenger(VkInstance instance, const VkAllocationCallbacks* allocator);
	static void									DestroyDebugMessenger(VkDebugUtilsMessengerEXT messenger, VkInstance instance, const VkAllocationCallbacks* allocator);
};

}