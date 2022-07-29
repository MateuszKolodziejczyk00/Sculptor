#include "RHIToVulkanCommon.h"

namespace spt::vulkan
{

VkPipelineStageFlags2 RHIToVulkan::GetStageFlags(rhi::EPipelineStage::Flags flags)
{
	VkPipelineStageFlags2 vulkanFlags = 0;

	if ((flags & rhi::EPipelineStage::TopOfPipe) != 0)
	{
		vulkanFlags |= VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
	}
	if ((flags & rhi::EPipelineStage::DrawIndirect) != 0)
	{
		vulkanFlags |= VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT;
	}
	if ((flags & rhi::EPipelineStage::VertexShader) != 0)
	{
		vulkanFlags |= VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT;
	}
	if ((flags & rhi::EPipelineStage::FragmentShader) != 0)
	{
		vulkanFlags |= VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
	}
	if ((flags & rhi::EPipelineStage::EarlyFragmentTest) != 0)
	{
		vulkanFlags |= VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT;
	}
	if ((flags & rhi::EPipelineStage::LateFragmentTest) != 0)
	{
		vulkanFlags |= VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
	}
	if ((flags & rhi::EPipelineStage::ColorRTOutput) != 0)
	{
		vulkanFlags |= VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
	}
	if ((flags & rhi::EPipelineStage::ComputeShader) != 0)
	{
		vulkanFlags |= VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
	}
	if ((flags & rhi::EPipelineStage::Transfer) != 0)
	{
		vulkanFlags |= VK_PIPELINE_STAGE_2_TRANSFER_BIT;
	}
	if ((flags & rhi::EPipelineStage::BottomOfPipe) != 0)
	{
		vulkanFlags |= VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
	}
	if ((flags & rhi::EPipelineStage::Copy) != 0)
	{
		vulkanFlags |= VK_PIPELINE_STAGE_2_COPY_BIT;
	}
	if ((flags & rhi::EPipelineStage::Blit) != 0)
	{
		vulkanFlags |= VK_PIPELINE_STAGE_2_BLIT_BIT;
	}
	if ((flags & rhi::EPipelineStage::Clear) != 0)
	{
		vulkanFlags |= VK_PIPELINE_STAGE_2_CLEAR_BIT;
	}
	if ((flags & rhi::EPipelineStage::IndexInput) != 0)
	{
		vulkanFlags |= VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT;
	}
	if ((flags & rhi::EPipelineStage::ALL_GRAPHICS) != 0)
	{
		vulkanFlags |= VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT;
	}
	if ((flags & rhi::EPipelineStage::ALL_TRANSFER) != 0)
	{
		vulkanFlags |= VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT;
	}
	if ((flags & rhi::EPipelineStage::ALL_COMMANDS) != 0)
	{
		vulkanFlags |= VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
	}

	return vulkanFlags;
}

VkImageLayout RHIToVulkan::GetImageLayout(rhi::ETextureLayout layout)
{
	switch (layout)
	{
	case rhi::ETextureLayout::Undefined:						return VK_IMAGE_LAYOUT_UNDEFINED;
	case rhi::ETextureLayout::General:							return VK_IMAGE_LAYOUT_GENERAL;
	case rhi::ETextureLayout::ColorRTOptimal:					return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	case rhi::ETextureLayout::DepthRTOptimal:					return VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
	case rhi::ETextureLayout::DepthStencilRTOptimal:			return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	case rhi::ETextureLayout::DepthRTStencilReadOptimal:		return VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL;
	case rhi::ETextureLayout::DepthReadOnlyStencilRTOptimal:	return VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL;
	case rhi::ETextureLayout::TransferSrcOptimal:				return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	case rhi::ETextureLayout::TransferDstOptimal:				return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	case rhi::ETextureLayout::DepthReadOnlyOptimal:				return VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL;
	case rhi::ETextureLayout::DepthStencilReadOnlyOptimal:		return VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
	case rhi::ETextureLayout::ColorReadOnlyOptimal:				return VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL;
	case rhi::ETextureLayout::PresentSrc:						return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	case rhi::ETextureLayout::ReadOnlyOptimal:					return VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL;
	case rhi::ETextureLayout::RenderTargetOptimal:				return VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
	}

	SPT_CHECK_NO_ENTRY();
	return VK_IMAGE_LAYOUT_UNDEFINED;
}

}
