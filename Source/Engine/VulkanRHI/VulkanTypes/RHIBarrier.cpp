#include "RHIBarrier.h"
#include "RHITexture.h"
#include "RHIToVulkanCommon.h"

namespace spt::vulkan
{

namespace priv
{

constexpr VkImageLayout g_uninitializedLayoutValue = VK_IMAGE_LAYOUT_MAX_ENUM;

void FillImageLayoutTransitionFlags(VkImageLayout layout, VkPipelineStageFlags2& outStage, VkAccessFlags2& outAccessFlags)
{
	switch (layout)
	{
	case VK_IMAGE_LAYOUT_UNDEFINED:
		outStage		= VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
		outAccessFlags	= VK_ACCESS_2_NONE;

		break;
	case VK_IMAGE_LAYOUT_GENERAL:
		outStage		= VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
		outAccessFlags	= VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT;

		break;
	case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
		outStage		= VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
		outAccessFlags	= VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
		
		break;
	case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL:
		outStage		= VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
		outAccessFlags	= VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		
		break;
	case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
		outStage		= VK_PIPELINE_STAGE_TRANSFER_BIT;
		outAccessFlags	= VK_ACCESS_2_TRANSFER_READ_BIT;
		
		break;
	case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
		outStage		= VK_PIPELINE_STAGE_TRANSFER_BIT;
		outAccessFlags	= VK_ACCESS_2_TRANSFER_WRITE_BIT;
		
		break;
	case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
		outStage		= VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
		outAccessFlags	= VK_ACCESS_SHADER_READ_BIT;

		break;
	case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
		outStage		= VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
		outAccessFlags	= VK_ACCESS_SHADER_READ_BIT;

		break;
	case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
		outStage		= VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
		outAccessFlags	= 0;
		
		break;
	default:
		SPT_CHECK_NO_ENTRY();
		break;
	}
}

}

RHIBarrier::RHIBarrier()
{ }

SizeType RHIBarrier::AddTextureBarrier(const RHITexture& texture, rhi::TextureSubresourceRange& subresourceRange)
{
	VkImageMemoryBarrier2 barrier{ VK_STRUCTURE_TYPE_MEMORY_BARRIER_2 };
	barrier.image								= texture.GetHandle();
	barrier.subresourceRange.aspectMask			= RHIToVulkan::GetAspectFlags(subresourceRange.m_aspect);
	barrier.subresourceRange.baseMipLevel		= subresourceRange.m_baseMipLevel;
	barrier.subresourceRange.levelCount			= subresourceRange.m_mipLevelsNum;
	barrier.subresourceRange.baseArrayLayer		= subresourceRange.m_baseArrayLayer;
	barrier.subresourceRange.layerCount			= subresourceRange.m_arrayLayersNum;
    barrier.srcStageMask						= VK_PIPELINE_STAGE_2_NONE;
    barrier.srcAccessMask						= VK_ACCESS_2_NONE;
    barrier.dstStageMask						= VK_PIPELINE_STAGE_2_NONE;
    barrier.dstAccessMask						= VK_ACCESS_2_NONE;
    barrier.oldLayout							= priv::g_uninitializedLayoutValue;
    barrier.newLayout							= priv::g_uninitializedLayoutValue;
    barrier.srcQueueFamilyIndex					= VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex					= VK_QUEUE_FAMILY_IGNORED;

	m_textureBarriers.push_back(barrier);

	return m_textureBarriers.size() - 1;
}

void RHIBarrier::SetLayoutTransition(SizeType barrierIdx, const rhi::BarrierTextureTransitionTarget& transitionTarget)
{
	SPT_CHECK(barrierIdx < m_textureBarriers.size());


}

void RHIBarrier::Execute(const RHICommandBuffer& cmdBuffer)
{
	SPT_PROFILE_FUNCTION();

	PrepareLayoutTransitionsForCommandBuffer(cmdBuffer);
}

void RHIBarrier::PrepareLayoutTransitionsForCommandBuffer(const RHICommandBuffer& cmdBuffer)
{

}

}
