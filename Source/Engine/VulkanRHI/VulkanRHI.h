#pragma once

#include "VulkanRHIMacros.h"
#include "Vulkan.h"
#include "RHIInitialization.h"


namespace spt::vulkan
{

class VULKANRHI_API VulkanRHI
{
public:

	static void Initialize(const rhicore::RHIInitializationInfo& InitInfo);

	static void Uninitialize();

private:

	static const VkAllocationCallbacks* GetAllocationCallbacks();
};

}