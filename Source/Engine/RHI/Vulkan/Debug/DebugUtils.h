#pragma once

#include "Vulkan/VulkanCore.h"
#include "SculptorCoreTypes.h"


namespace spt::vulkan
{

class DebugUtils
{
public:

#if SPT_RHI_DEBUG

	static void SetObjectName(VkDevice device, Uint64 object, VkObjectType objectType, const char* name);

#endif // SPT_RHI_DEBUG
};


class DebugName
{
public:

	DebugName();

	void						Set(const lib::HashedString& name, Uint64 object, VkObjectType objectType);

	void						SetWithoutObject(const lib::HashedString& name);
	void						SetToObject(Uint64 object, VkObjectType objectType) const;

	const lib::HashedString&	Get() const;

	Bool						HasName() const;

	void						Reset(Uint64 object, VkObjectType objectType);

private:

#if SPT_RHI_DEBUG
	lib::HashedString m_name;
#endif // SPT_RHI_DEBUG
};

}
