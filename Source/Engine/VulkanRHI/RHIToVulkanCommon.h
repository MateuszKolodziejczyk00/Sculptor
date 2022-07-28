#pragma once

#include "Vulkan.h"
#include "RHISemaphoreTypes.h"


namespace spt::vulkan
{

class RHIToVulkan
{
public:

	static VkPipelineStageFlags2		GetStageFlags(rhi::EPipelineStage::Flags flags);
};

}