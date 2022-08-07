#include "DebugUtils.h"
#include "Vulkan/VulkanRHI.h"

namespace spt::vulkan
{

#if VULKAN_VALIDATION

void DebugUtils::SetObjectName(VkDevice device, Uint64 object, VkObjectType objectType, const char* name)
{
	SPT_PROFILE_FUNCTION();

	VkDebugUtilsObjectNameInfoEXT objectNameInfo{ VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT };
    objectNameInfo.objectType = objectType;
    objectNameInfo.objectHandle = object;
    objectNameInfo.pObjectName = name;

    SPT_VK_CHECK(vkSetDebugUtilsObjectNameEXT(device, &objectNameInfo));
}

#endif // VULKAN_VALIDATION

DebugName::DebugName()
{ }

void DebugName::Set(const lib::HashedString& name, Uint64 object, VkObjectType objectType)
{
#if VULKAN_VALIDATION

    m_name = name;
    DebugUtils::SetObjectName(VulkanRHI::GetDeviceHandle(), object, objectType, name.GetData());

#endif // VULKAN_VALIDATION
}

const spt::lib::HashedString& DebugName::Get() const
{
#if VULKAN_VALIDATION

    return m_name;

#else

    static lib::HashedString dummyName;
    return dummyName;

#endif // VULKAN_VALIDATION

}

void DebugName::Reset()
{
#if VULKAN_VALIDATION

    m_name.Reset();

#endif // VULKAN_VALIDATION
}

}
