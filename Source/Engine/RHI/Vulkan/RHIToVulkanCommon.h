#pragma once

#include "Vulkan/VulkanCore.h"
#include "RHICore/RHISemaphoreTypes.h"
#include "RHICore/RHITextureTypes.h"
#include "RHICore/RHIPipelineTypes.h"


namespace spt::vulkan
{

class RHIToVulkan
{
public:

	static VkPipelineStageFlags2		GetStageFlags(Flags32 flags);

	static VkImageLayout				GetImageLayout(rhi::ETextureLayout layout);

	static VkImageAspectFlags			GetAspectFlags(Flags32 flags);
};

}