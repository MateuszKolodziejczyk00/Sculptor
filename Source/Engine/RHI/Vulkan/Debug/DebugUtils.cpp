#include "DebugUtils.h"
#include "Vulkan/VulkanRHI.h"

namespace spt::vulkan
{

#if SPT_RHI_DEBUG

void DebugUtils::SetObjectName(VkDevice device, Uint64 object, VkObjectType objectType, const char* name)
{
	SPT_PROFILER_FUNCTION();

	VkDebugUtilsObjectNameInfoEXT objectNameInfo{ VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT };
    objectNameInfo.objectType = objectType;
    objectNameInfo.objectHandle = object;
    objectNameInfo.pObjectName = name;

    SPT_VK_CHECK(vkSetDebugUtilsObjectNameEXT(device, &objectNameInfo));
}

#endif // SPT_RHI_DEBUG

DebugName::DebugName()
{ }

void DebugName::Set(const lib::HashedString& name, Uint64 object, VkObjectType objectType)
{
#if SPT_RHI_DEBUG

    m_name = name;
    SetToObject(object, objectType);

#endif // SPT_RHI_DEBUG
}

void DebugName::SetWithoutObject(const lib::HashedString& name)
{
#if SPT_RHI_DEBUG

    m_name = name;

#endif // SPT_RHI_DEBUG
}

void DebugName::SetToObject(Uint64 object, VkObjectType objectType) const
{
#if SPT_RHI_DEBUG

    DebugUtils::SetObjectName(VulkanRHI::GetDeviceHandle(), object, objectType, m_name.GetData());

#endif // SPT_RHI_DEBUG
}

const spt::lib::HashedString& DebugName::Get() const
{
#if SPT_RHI_DEBUG

    return m_name;

#else

    static const lib::HashedString dummyName{};
    return dummyName;

#endif // SPT_RHI_DEBUG
}

Bool DebugName::HasName() const
{
#if SPT_RHI_DEBUG

    return m_name.IsValid();

#else

    return false;

#endif // SPT_RHI_DEBUG
}

void DebugName::Reset(Uint64 object, VkObjectType objectType)
{
#if SPT_RHI_DEBUG

    if (HasName() && object != 0)
    {
        if (!VulkanRHI::GetSettings().ArePersistentDebugNamesEnabled())
        {
            DebugUtils::SetObjectName(VulkanRHI::GetDeviceHandle(), object, objectType, nullptr);
        }
    }
    
    m_name.Reset();

#endif // SPT_RHI_DEBUG
}

} // spt::vulkan
