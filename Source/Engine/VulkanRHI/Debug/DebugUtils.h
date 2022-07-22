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


class DebugName
{
public:

	DebugName();

	void						Set(const lib::HashedString& name, Uint64 object, VkObjectType objectType);
	const lib::HashedString&	Get() const;

	void						Reset();

private:

#if VULKAN_VALIDATION
	lib::HashedString m_name;
#endif // VULKAN_VALIDATION
};

}
