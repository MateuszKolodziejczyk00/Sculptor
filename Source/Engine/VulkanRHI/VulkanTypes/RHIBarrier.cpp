#include "RHIBarrier.h"
#include "RHITexture.h"
#include "RHIToVulkanCommon.h"

namespace spt::vulkan
{

namespace priv
{

constexpr VkImageLayout g_uninitializedLayoutValue = VK_IMAGE_LAYOUT_MAX_ENUM;

void FillImageLayoutTransitionFlags(rhi::ETextureLayout layout, VkPipelineStageFlags2& outStage, VkAccessFlags2& outAccessFlags, VkImageLayout& outLayout)
{
	switch (layout)
	{
	case spt::rhi::ETextureLayout::Undefined:
		outStage		= VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
		outAccessFlags	= VK_ACCESS_2_NONE;
		outLayout		= VK_IMAGE_LAYOUT_UNDEFINED;

		break;
	case spt::rhi::ETextureLayout::ComputeGeneral:
		outStage		= VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
		outAccessFlags	= VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT;
		outLayout		= VK_IMAGE_LAYOUT_GENERAL;

		break;
	case spt::rhi::ETextureLayout::FragmentGeneral:
		outStage		= VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
		outAccessFlags	= VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT;
		outLayout		= VK_IMAGE_LAYOUT_GENERAL;

		break;
	case spt::rhi::ETextureLayout::ColorRTOptimal:
		outStage		= VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
		outAccessFlags	= VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
		outLayout		= VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		
		break;
	case spt::rhi::ETextureLayout::DepthRTOptimal:
		outStage		= VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
		outAccessFlags	= VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		outLayout		= VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
		
		break;/*
	case spt::rhi::ETextureLayout::DepthStencilRTOptimal:
		outStage		= ;
		outAccessFlags	= ;
		outLayout		= ;
		
		break;
	case spt::rhi::ETextureLayout::DepthRTStencilReadOptimal:
		outStage		= ;
		outAccessFlags	= ;
		outLayout		= ;
		
		break;
	case spt::rhi::ETextureLayout::DepthReadOnlyStencilRTOptimal:
		outStage		= ;
		outAccessFlags	= ;
		outLayout		= ;
		
		break;*/
	case spt::rhi::ETextureLayout::TransferSrcOptimal:
		outStage		= VK_PIPELINE_STAGE_TRANSFER_BIT;
		outAccessFlags	= VK_ACCESS_2_TRANSFER_READ_BIT;
		outLayout		= VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		
		break;
	case spt::rhi::ETextureLayout::TransferDstOptimal:
		outStage		= VK_PIPELINE_STAGE_TRANSFER_BIT;
		outAccessFlags	= VK_ACCESS_2_TRANSFER_WRITE_BIT;
		outLayout		= VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		
		break;/*
	case spt::rhi::ETextureLayout::DepthReadOnlyOptimal:
		outStage		= ;
		outAccessFlags	= ;
		outLayout		= ;
		
		break;
	case spt::rhi::ETextureLayout::DepthStencilReadOnlyOptimal:
		outStage		= ;
		outAccessFlags	= ;
		outLayout		= ;
		
		break;
	case spt::rhi::ETextureLayout::ColorReadOnlyOptimal:
		outStage		= ;
		outAccessFlags	= ;
		outLayout		= ;
		
		break;*/
	case spt::rhi::ETextureLayout::PresentSrc:
		outStage		= VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
		outAccessFlags	= 0;
		outLayout		= VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		
		break;/*
	case spt::rhi::ETextureLayout::ReadOnlyOptimal:
		outStage		= ;
		outAccessFlags	= ;
		outLayout		= ;
		
		break;
	case spt::rhi::ETextureLayout::RenderTargetOptimal:
		outStage		= ;
		outAccessFlags	= ;
		outLayout		= ;
		
		break;*/
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

void RHIBarrier::SetLayoutTransition(SizeType barrierIdx, rhi::ETextureLayout targetLayout)
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
