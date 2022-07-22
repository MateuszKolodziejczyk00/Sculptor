#pragma once

#include "Vulkan.h"
#include "SculptorCoreTypes.h"


namespace spt::vulkan
{

class DebugUtils
{
public:

#if VULKAN_VALIDATION

	static void SetObjectName(VkDevice device, Uint64 object, VkObjectType objectType, const char* name);

#endif // VULKAN_VALIDATION
};

}