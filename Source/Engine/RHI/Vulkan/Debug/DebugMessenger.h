#pragma once

#include "Vulkan/Vulkan123.h"

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