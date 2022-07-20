#pragma once

#include "Vulkan.h"


namespace spt::vulkan
{

struct LogicalDevice
{
public:

	VkDevice CreateLogicalDevice(VkPhysicalDevice physicalDevice);
	
};

}