#pragma once

#include "VulkanRHIMacros.h"


namespace spt::vulkan
{

class VULKANRHI_API VulkanRHI
{
public:

	static void Initialize();

	static void Uninitialize();
};

}