#pragma once

#include "Vulkan/VulkanCore.h"
#include "RHICore/RHISemaphoreTypes.h"
#include "RHICore/RHITextureTypes.h"
#include "RHICore/RHIPipelineTypes.h"
#include "RHICore/RHIPipelineLayoutTypes.h"
#include "RHICore/Commands/RHIRenderingDefinition.h"
#include "RHICore/RHIPipelineDefinitionTypes.h"
#include "RHICore/RHISamplerTypes.h"
#include "RHICore/RHISynchronizationTypes.h"
#include "RHICore/RHIAccelerationStructureTypes.h"
#include "RHICore/RHIQueryTypes.h"


namespace spt::vulkan
{

class RHIToVulkan
{
public:

	static VkPipelineStageFlags2			GetStageFlags(rhi::EPipelineStage flags);
	static VkPipelineStageFlags				GetStageFlagsLegacy(rhi::EPipelineStage flags);

	static VkImageLayout					GetImageLayout(rhi::ETextureLayout layout);

	static VkFormat							GetVulkanFormat(rhi::EFragmentFormat format);

	static VkImageAspectFlags				GetAspectFlags(rhi::ETextureAspect flags);

	static VkRenderingFlags					GetRenderingFlags(rhi::ERenderingFlags flags);

	static VkAttachmentLoadOp				GetLoadOp(rhi::ERTLoadOperation loadOp);
	static VkAttachmentStoreOp				GetStoreOp(rhi::ERTStoreOperation storeOp);

	static VkResolveModeFlagBits			GetResolveMode(rhi::ERTResolveMode resolveMode);

	static VkDescriptorType					GetDescriptorType(rhi::EDescriptorType descriptorType);

	static VkDescriptorBindingFlags			GetBindingFlags(rhi::EDescriptorSetBindingFlags bindingFlags);

	static VkDescriptorSetLayoutCreateFlags	GetDescriptorSetFlags(rhi::EDescriptorSetFlags dsFlags);

	static VkShaderStageFlagBits			GetShaderStage(rhi::EShaderStage stage);
	static VkShaderStageFlags				GetShaderStages(rhi::EShaderStageFlags stages);

	static VkPrimitiveTopology				GetPrimitiveTopology(rhi::EPrimitiveTopology topology);

	static VkCullModeFlags					GetCullMode(rhi::ECullMode cullMode);

	static VkPolygonMode					GetPolygonMode(rhi::EPolygonMode polygonMode);

	static VkSampleCountFlagBits			GetSampleCount(Uint32 samplesNum);

	static VkCompareOp						GetCompareOp(rhi::ECompareOp compareOperation);

	static VkColorComponentFlags			GetColorComponentFlags(rhi::ERenderTargetComponentFlags componentFlags);

	static VkDescriptorSetLayout			GetDSLayout(rhi::DescriptorSetLayoutID layoutID);
	static const VkDescriptorSetLayout*		GetDSLayoutsPtr(const rhi::DescriptorSetLayoutID* layoutIDs);

    static VkSamplerCreateFlags				GetSamplerCreateFlags(rhi::ESamplerFlags flags);

    static VkFilter							GetSamplerFilterType(rhi::ESamplerFilterType type);

    static VkSamplerMipmapMode				GetMipMapAddressingMode(rhi::EMipMapAddressingMode mode);

	static VkSamplerAddressMode				GetAxisAddressingMode(rhi::EAxisAddressingMode mode);

	static VkSamplerReductionMode			GetSamplerReductionMode(rhi::ESamplerReductionMode mode);

	static VkBorderColor					GetBorderColor(rhi::EBorderColor color);

	static VkEventCreateFlags				GetEventFlags(rhi::EEventFlags eventFlags);

	static VkGeometryFlagsKHR				GetGeometryFlags(rhi::EGeometryFlags geometryFlags);

	static VkGeometryInstanceFlagsKHR		GetGeometryInstanceFlags(rhi::ETLASInstanceFlags instanceFlags);

	static VkQueryType						GetQueryType(rhi::EQueryType queryType);

	static VkQueryPipelineStatisticFlags	GetPipelineStatistic(rhi::EQueryStatisticsType statisticsType);
};

struct VulkanToRHI
{
public:

	static rhi::DescriptorSetLayoutID			GetDSLayoutID(VkDescriptorSetLayout layoutHandle);
	static const rhi::DescriptorSetLayoutID*	GetDSLayoutIDsPtr(VkDescriptorSetLayout layoutHandle);
};

} // rhi::vulkan