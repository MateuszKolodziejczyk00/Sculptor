#include "RHIBarrier.h"
#include "RHITexture.h"
#include "RHICommandBuffer.h"
#include "Vulkan/RHIToVulkanCommon.h"
#include "Vulkan/VulkanRHI.h"
#include "Vulkan/LayoutsManager.h"

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

VkAccessFlags2 GetVulkanAccessFlags(const rhi::BarrierTextureTransitionTarget& transitionTarget)
{
	const Flags32 rhiFlags = transitionTarget.m_accessType;

	VkAccessFlags2 flags = 0;

	switch (transitionTarget.m_layout)
	{
	case rhi::ETextureLayout::Undefined:
	case rhi::ETextureLayout::General:
	case rhi::ETextureLayout::DepthReadOnlyStencilRTOptimal:
	case rhi::ETextureLayout::DepthReadOnlyOptimal:
	case rhi::ETextureLayout::DepthStencilReadOnlyOptimal:
	case rhi::ETextureLayout::ColorReadOnlyOptimal:
	case rhi::ETextureLayout::PresentSrc:
	case rhi::ETextureLayout::ReadOnlyOptimal:
	case rhi::ETextureLayout::RenderTargetOptimal:
		if ((rhiFlags & rhi::EAccessType::Read) != 0)
		{
			flags |= VK_ACCESS_2_SHADER_READ_BIT;
		}
		if ((rhiFlags & rhi::EAccessType::Write) != 0)
		{
			flags |= VK_ACCESS_2_SHADER_WRITE_BIT;
		}

		break;

	case rhi::ETextureLayout::ColorRTOptimal:
		if ((rhiFlags & rhi::EAccessType::Read) != 0)
		{
			flags |= VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT;
		}
		if ((rhiFlags & rhi::EAccessType::Write) != 0)
		{
			flags |= VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
		}

		break;

	case rhi::ETextureLayout::DepthRTOptimal:
	case rhi::ETextureLayout::DepthStencilRTOptimal:
	case rhi::ETextureLayout::DepthRTStencilReadOptimal:
		if ((rhiFlags & rhi::EAccessType::Read) != 0)
		{
			flags |= VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
		}
		if ((rhiFlags & rhi::EAccessType::Write) != 0)
		{
			flags |= VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		}

		break;

	case rhi::ETextureLayout::TransferSrcOptimal:
	case rhi::ETextureLayout::TransferDstOptimal:
		if ((rhiFlags & rhi::EAccessType::Read) != 0)
		{
			flags |= VK_ACCESS_2_TRANSFER_READ_BIT;
		}
		if ((rhiFlags & rhi::EAccessType::Write) != 0)
		{
			flags |= VK_ACCESS_2_TRANSFER_WRITE_BIT;
		}

		break;

	default:

		SPT_CHECK_NO_ENTRY(); //  Missing layout?
	}

	return flags;
}

}

RHIBarrier::RHIBarrier()
{ }

SizeType RHIBarrier::AddTextureBarrier(const RHITexture& texture, rhi::TextureSubresourceRange& subresourceRange)
{
	VkImageMemoryBarrier2 barrier{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
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

	VkImageMemoryBarrier2& barrier = m_textureBarriers[barrierIdx];
    barrier.dstStageMask	= RHIToVulkan::GetStageFlags(transitionTarget.m_stage);
    barrier.dstAccessMask	= priv::GetVulkanAccessFlags(transitionTarget);
    barrier.newLayout		= RHIToVulkan::GetImageLayout(transitionTarget.m_layout);
}

void RHIBarrier::Execute(const RHICommandBuffer& cmdBuffer)
{
	SPT_PROFILE_FUNCTION();

	PrepareLayoutTransitionsForCommandBuffer(cmdBuffer);

	VkDependencyInfo dependency{ VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
    dependency.imageMemoryBarrierCount	= static_cast<Uint32>(m_textureBarriers.size());
    dependency.pImageMemoryBarriers		= m_textureBarriers.data();
    dependency.bufferMemoryBarrierCount = static_cast<Uint32>(m_bufferBarriers.size());
    dependency.pBufferMemoryBarriers	= m_bufferBarriers.data();

	vkCmdPipelineBarrier2(cmdBuffer.GetHandle(), &dependency);

	WriteNewLayoutsToLayoutsManager(cmdBuffer);
}

void RHIBarrier::PrepareLayoutTransitionsForCommandBuffer(const RHICommandBuffer& cmdBuffer)
{
	SPT_PROFILE_FUNCTION();

	const LayoutsManager& layoutsManager = VulkanRHI::GetLayoutsManager();

	for (VkImageMemoryBarrier2& textureBarrier : m_textureBarriers)
	{
		textureBarrier.oldLayout = layoutsManager.GetSubresourcesSharedLayout(cmdBuffer.GetHandle(), textureBarrier.image, textureBarrier.subresourceRange);

		// fill stage and access masks based on old layout
		priv::FillImageLayoutTransitionFlags(textureBarrier.oldLayout, textureBarrier.srcStageMask, textureBarrier.srcAccessMask);
	}
}

void RHIBarrier::WriteNewLayoutsToLayoutsManager(const RHICommandBuffer& cmdBuffer)
{
	SPT_PROFILE_FUNCTION();

	LayoutsManager& layoutsManager = VulkanRHI::GetLayoutsManager();

	for (VkImageMemoryBarrier2& textureBarrier : m_textureBarriers)
	{
		layoutsManager.SetSubresourcesLayout(cmdBuffer.GetHandle(), textureBarrier.image, textureBarrier.subresourceRange, textureBarrier.newLayout);
	}
}

}
