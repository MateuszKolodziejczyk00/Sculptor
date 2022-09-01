#pragma once

#include "Vulkan/VulkanCore.h"
#include "RHICore/RHISemaphoreTypes.h"
#include "RHICore/RHITextureTypes.h"
#include "RHICore/RHIPipelineTypes.h"
#include "RHICore/RHIPipelineLayoutTypes.h"
#include "RHICore/Commands/RHIRenderingDefinition.h"


namespace spt::vulkan
{

class RHIToVulkan
{
public:

	static VkPipelineStageFlags2			GetStageFlags(rhi::EPipelineStage flags);
	static VkPipelineStageFlags				GetStageFlagsLegacy(rhi::EPipelineStage flags);
	static VkPipelineStageFlags				GetStageFlagsLegacy(rhi::EShaderStageFlags flags);

	static VkImageLayout					GetImageLayout(rhi::ETextureLayout layout);

	static VkImageAspectFlags				GetAspectFlags(rhi::ETextureAspect flags);

	static VkRenderingFlags					GetRenderingFlags(rhi::ERenderingFlags flags);

	static VkAttachmentLoadOp				GetLoadOp(rhi::ERTLoadOperation loadOp);
	static VkAttachmentStoreOp				GetStoreOp(rhi::ERTStoreOperation storeOp);

	static VkResolveModeFlagBits			GetResolveMode(rhi::ERTResolveMode resolveMode);

	static VkDescriptorType					GetDescriptorType(rhi::EDescriptorType descriptorType);

	static VkDescriptorBindingFlags			GetBindingFlags(rhi::EDescriptorSetBindingFlags bindingFlags);

	static VkDescriptorSetLayoutCreateFlags	GetDescriptorSetFlags(rhi::EDescriptorSetFlags dsFlags);

	static VkShaderStageFlagBits			GetShaderStage(rhi::EShaderStage stage);
};

}