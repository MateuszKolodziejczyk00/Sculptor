#pragma once

#include "SculptorCoreTypes.h"


namespace spt::vulkan
{

struct VulkanDeviceCommon
{
	static lib::DynamicArray<const char*> GetRequiredDeviceExtensions();
};

} // spt::vulkan