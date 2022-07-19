#pragma once

#include "VulkanRHIMacros.h"
#include "Vulkan.h"
#include "RHIInitialization.h"


namespace spt::vulkan
{

class VULKANRHI_API VulkanRHI
{
public:

	// RHI Interface ===================================================================

	static void Initialize(const rhicore::RHIInitializationInfo& InitInfo);

	static void SelectAndInitializeGPU();

	static void Uninitialize();


	// Vulkan Getters ==================================================================

	static VkInstance GetVulkanInstance();

	static void SetVulkanSurface(VkSurfaceKHR surface);

	static const VkAllocationCallbacks* GetAllocationCallbacks();
};

}