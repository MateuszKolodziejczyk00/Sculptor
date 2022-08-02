#pragma once

#include "Vulkan.h"
#include "RHISemaphoreTypes.h"
#include "RHITextureTypes.h"


namespace spt::vulkan
{

class RHIToVulkan
{
public:

	static VkPipelineStageFlags2		GetStageFlags(rhi::EPipelineStage::Flags flags);

	static VkImageLayout				GetImageLayout(rhi::ETextureLayout layout);

	static VkImageAspectFlags			GetAspectFlags(Flags32 flags);
};

}