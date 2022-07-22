#include "DebugUtils.h"

namespace spt::vulkan
{

#if VULKAN_VALIDATION

void DebugUtils::SetObjectName(VkDevice device, Uint64 object, VkObjectType objectType, const char* name)
{
	VkDebugUtilsObjectNameInfoEXT objectNameInfo{ VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT };
    objectNameInfo.objectType = objectType;
    objectNameInfo.objectHandle = object;
    objectNameInfo.pObjectName = name;

    SPT_VK_CHECK(vkSetDebugUtilsObjectNameEXT(device, &objectNameInfo));
}

#endif // VULKAN_VALIDATION

}
