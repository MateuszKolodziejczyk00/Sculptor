#include "DebugUtils.h"
#include "Vulkan/VulkanRHI.h"

namespace spt::vulkan
{

#if RHI_DEBUG

void DebugUtils::SetObjectName(VkDevice device, Uint64 object, VkObjectType objectType, const char* name)
{
	SPT_PROFILER_FUNCTION();

	VkDebugUtilsObjectNameInfoEXT objectNameInfo{ VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT };
    objectNameInfo.objectType = objectType;
    objectNameInfo.objectHandle = object;
    objectNameInfo.pObjectName = name;

    SPT_VK_CHECK(vkSetDebugUtilsObjectNameEXT(device, &objectNameInfo));
}

#endif // RHI_DEBUG

DebugName::DebugName()
{ }

void DebugName::Set(const lib::HashedString& name, Uint64 object, VkObjectType objectType)
{
#if RHI_DEBUG

    m_name = name;
    SetToObject(object, objectType);

#endif // RHI_DEBUG
}

void DebugName::SetWithoutObject(const lib::HashedString& name)
{
#if RHI_DEBUG

    m_name = name;

#endif // RHI_DEBUG
}

void DebugName::SetToObject(Uint64 object, VkObjectType objectType) const
{
#if RHI_DEBUG

    DebugUtils::SetObjectName(VulkanRHI::GetDeviceHandle(), object, objectType, m_name.GetData());

#endif // RHI_DEBUG
}

const spt::lib::HashedString& DebugName::Get() const
{
#if RHI_DEBUG

    return m_name;

#else

    static const lib::HashedString dummyName{};
    return dummyName;

#endif // RHI_DEBUG
}

Bool DebugName::HasName() const
{
#if RHI_DEBUG

    return m_name.IsValid();

#else

    return false;

#endif // RHI_DEBUG
}

void DebugName::Reset(Uint64 object /*= 0*/, VkObjectType objectType /*= VK_OBJECT_TYPE_UNKNOWN*/)
{
#if RHI_DEBUG

    if (HasName() && object != 0)
    {
        DebugUtils::SetObjectName(VulkanRHI::GetDeviceHandle(), object, objectType, nullptr);
    }
    
    m_name.Reset();

#endif // RHI_DEBUG
}

} // spt::vulkan
