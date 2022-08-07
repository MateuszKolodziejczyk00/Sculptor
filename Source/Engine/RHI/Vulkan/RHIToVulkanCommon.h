#pragma once

#include "Vulkan/VulkanCore.h"
#include "RHICore/RHISemaphoreTypes.h"
#include "RHICore/RHITextureTypes.h"
#include "RHICore/RHIPipelineTypes.h"
#include "RHICore/Commands/RHIRenderingDefinition.h"


namespace spt::vulkan
{

class RHIToVulkan
{
public:

	static VkPipelineStageFlags2		GetStageFlags(Flags32 flags);

	static VkImageLayout				GetImageLayout(rhi::ETextureLayout layout);

	static VkImageAspectFlags			GetAspectFlags(Flags32 flags);

	static VkRenderingFlags				GetRenderingFlags(Flags32 flags);

	static VkAttachmentLoadOp			GetLoadOp(rhi::ERTLoadOperation loadOp);
	static VkAttachmentStoreOp			GetStoreOp(rhi::ERTStoreOperation storeOp);

	static VkResolveModeFlagBits		GetResolveMode(rhi::ERTResolveMode resolveMode);
};

}