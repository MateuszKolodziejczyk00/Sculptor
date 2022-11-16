#include "Vulkan/VulkanTypes/RHIDependency.h"
#include "Vulkan/LayoutsManager.h"
#include "Vulkan/VulkanTypes/RHITexture.h"
#include "Vulkan/VulkanTypes/RHICommandBuffer.h"
#include "Vulkan/VulkanRHIUtils.h"
#include "Vulkan/VulkanRHI.h"
#include "Vulkan/VulkanTypes/RHIEvent.h"

namespace spt::vulkan
{

namespace priv
{

constexpr VkImageLayout g_uninitializedLayoutValue = VK_IMAGE_LAYOUT_MAX_ENUM;

// adding 1 here should be fine (VK_IMAGE_LAYOUT_MAX_ENUM is not max value so it won't overflow)
constexpr VkImageLayout g_invalidLayoutValue = static_cast<VkImageLayout>(static_cast<Uint32>(VK_IMAGE_LAYOUT_MAX_ENUM) + 1);

static void FillImageLayoutTransitionFlags(VkImageLayout layout, VkPipelineStageFlags2& outStage, VkAccessFlags2& outAccessFlags)
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

static VkAccessFlags2 GetVulkanAccessFlags(const rhi::BarrierTextureTransitionTarget& transitionTarget)
{
	const rhi::EAccessType rhiFlags = transitionTarget.accessType;

	VkAccessFlags2 flags = 0;

	switch (transitionTarget.layout)
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
		if (lib::HasAnyFlag(rhiFlags, rhi::EAccessType::Read))
		{
			lib::AddFlag(flags, VK_ACCESS_2_SHADER_READ_BIT);
		}
		if (lib::HasAnyFlag(rhiFlags, rhi::EAccessType::Write))
		{
			lib::AddFlag(flags, VK_ACCESS_2_SHADER_WRITE_BIT);
		}

		break;

	case rhi::ETextureLayout::ColorRTOptimal:
		if (lib::HasAnyFlag(rhiFlags, rhi::EAccessType::Read))
		{
			lib::AddFlag(flags, VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT);
		}
		if (lib::HasAnyFlag(rhiFlags, rhi::EAccessType::Write))
		{
			lib::AddFlag(flags, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);
		}

		break;

	case rhi::ETextureLayout::DepthRTOptimal:
	case rhi::ETextureLayout::DepthStencilRTOptimal:
	case rhi::ETextureLayout::DepthRTStencilReadOptimal:
		if (lib::HasAnyFlag(rhiFlags, rhi::EAccessType::Read))
		{
			lib::AddFlag(flags, VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT);
		}
		if (lib::HasAnyFlag(rhiFlags, rhi::EAccessType::Write))
		{
			lib::AddFlag(flags, VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);
		}

		break;

	case rhi::ETextureLayout::TransferSrcOptimal:
	case rhi::ETextureLayout::TransferDstOptimal:
		if (lib::HasAnyFlag(rhiFlags, rhi::EAccessType::Read))
		{
			lib::AddFlag(flags, VK_ACCESS_2_TRANSFER_READ_BIT);
		}
		if (lib::HasAnyFlag(rhiFlags, rhi::EAccessType::Write))
		{
			lib::AddFlag(flags, VK_ACCESS_2_TRANSFER_WRITE_BIT);
		}

		break;

	default:

		SPT_CHECK_NO_ENTRY(); //  Missing layout?
	}

	return flags;
}

} // spt::priv

RHIDependency::RHIDependency()
{ }

Bool RHIDependency::IsEmpty() const
{
	return m_textureBarriers.empty() && m_bufferBarriers.empty();
}

SizeType RHIDependency::AddTextureDependency(const RHITexture& texture, const rhi::TextureSubresourceRange& subresourceRange)
{
	SPT_PROFILER_FUNCTION();

	const rhi::TextureDefinition& textureDef = texture.GetDefinition();
	const Bool isTrackingTextureLayout = !lib::HasAnyFlag(textureDef.flags, rhi::ETextureFlags::UntrackedLayout);

	const VkImageLayout initalLayout = isTrackingTextureLayout ? priv::g_uninitializedLayoutValue : priv::g_invalidLayoutValue;

	VkImageMemoryBarrier2 barrier{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
	barrier.image								= texture.GetHandle();
	barrier.subresourceRange.aspectMask			= RHIToVulkan::GetAspectFlags(subresourceRange.aspect);
	barrier.subresourceRange.baseMipLevel		= subresourceRange.baseMipLevel;
	barrier.subresourceRange.levelCount			= subresourceRange.mipLevelsNum;
	barrier.subresourceRange.baseArrayLayer		= subresourceRange.baseArrayLayer;
	barrier.subresourceRange.layerCount			= subresourceRange.arrayLayersNum;
    barrier.srcStageMask						= VK_PIPELINE_STAGE_2_NONE;
    barrier.srcAccessMask						= VK_ACCESS_2_NONE;
    barrier.dstStageMask						= VK_PIPELINE_STAGE_2_NONE;
    barrier.dstAccessMask						= VK_ACCESS_2_NONE;
	barrier.oldLayout							= initalLayout;
    barrier.newLayout							= initalLayout;
    barrier.srcQueueFamilyIndex					= VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex					= VK_QUEUE_FAMILY_IGNORED;

	m_textureBarriers.push_back(barrier);

	return m_textureBarriers.size() - 1;
}

void RHIDependency::SetLayoutTransition(SizeType barrierIdx, const rhi::BarrierTextureTransitionTarget& transitionTarget)
{
	SPT_CHECK(barrierIdx < m_textureBarriers.size());

	VkImageMemoryBarrier2& barrier = m_textureBarriers[barrierIdx];
    barrier.dstStageMask	= RHIToVulkan::GetStageFlags(transitionTarget.stage);
    barrier.dstAccessMask	= priv::GetVulkanAccessFlags(transitionTarget);
    barrier.newLayout		= RHIToVulkan::GetImageLayout(transitionTarget.layout);
}

void RHIDependency::SetLayoutTransition(SizeType barrierIdx, const rhi::BarrierTextureTransitionTarget& transitionSource, const rhi::BarrierTextureTransitionTarget& transitionTarget)
{
	SPT_CHECK(barrierIdx < m_textureBarriers.size());

	VkImageMemoryBarrier2& barrier = m_textureBarriers[barrierIdx];
    barrier.srcStageMask	= RHIToVulkan::GetStageFlags(transitionSource.stage);
    barrier.srcAccessMask	= priv::GetVulkanAccessFlags(transitionSource);
    barrier.oldLayout		= RHIToVulkan::GetImageLayout(transitionSource.layout);
    barrier.dstStageMask	= RHIToVulkan::GetStageFlags(transitionTarget.stage);
    barrier.dstAccessMask	= priv::GetVulkanAccessFlags(transitionTarget);
    barrier.newLayout		= RHIToVulkan::GetImageLayout(transitionTarget.layout);
}

void RHIDependency::ExecuteBarrier(const RHICommandBuffer& cmdBuffer)
{
	SPT_PROFILER_FUNCTION();

	PrepareLayoutTransitionsForCommandBuffer(cmdBuffer);

	const VkDependencyInfo dependencyInfo = GetDependencyInfo();
	vkCmdPipelineBarrier2(cmdBuffer.GetHandle(), &dependencyInfo);

	WriteNewLayoutsToLayoutsManager(cmdBuffer);
}

void RHIDependency::SetEvent(const RHICommandBuffer& cmdBuffer, const RHIEvent& event)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(event.IsValid());

	PrepareLayoutTransitionsForCommandBuffer(cmdBuffer);

	const VkDependencyInfo dependencyInfo = GetDependencyInfo();
	vkCmdSetEvent2(cmdBuffer.GetHandle(), event.GetHandle(), &dependencyInfo);
}

void RHIDependency::WaitEvent(const RHICommandBuffer& cmdBuffer, const RHIEvent& event)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(event.IsValid());

	const VkDependencyInfo dependencyInfo = GetDependencyInfo();

	const VkEvent eventHandle = event.GetHandle();
	vkCmdWaitEvents2(cmdBuffer, 1, &eventHandle, &dependencyInfo);

	WriteNewLayoutsToLayoutsManager(cmdBuffer);
}

void RHIDependency::PrepareLayoutTransitionsForCommandBuffer(const RHICommandBuffer& cmdBuffer)
{
	SPT_PROFILER_FUNCTION();

	const LayoutsManager& layoutsManager = VulkanRHI::GetLayoutsManager();

	for (VkImageMemoryBarrier2& textureBarrier : m_textureBarriers)
	{
		if (textureBarrier.oldLayout == priv::g_uninitializedLayoutValue)
		{
			textureBarrier.oldLayout = layoutsManager.GetSubresourcesSharedLayout(cmdBuffer.GetHandle(), textureBarrier.image, textureBarrier.subresourceRange);

			// fill stage and access masks based on old layout
			priv::FillImageLayoutTransitionFlags(textureBarrier.oldLayout, textureBarrier.srcStageMask, textureBarrier.srcAccessMask);
		}
	}
}

void RHIDependency::WriteNewLayoutsToLayoutsManager(const RHICommandBuffer& cmdBuffer)
{
	SPT_PROFILER_FUNCTION();

	LayoutsManager& layoutsManager = VulkanRHI::GetLayoutsManager();

	for (VkImageMemoryBarrier2& textureBarrier : m_textureBarriers)
	{
		layoutsManager.SetSubresourcesLayout(cmdBuffer.GetHandle(), textureBarrier.image, textureBarrier.subresourceRange, textureBarrier.newLayout);
	}
}

VkDependencyInfo RHIDependency::GetDependencyInfo() const
{
#if DO_CHECKS
	ValidateBarriers();
#endif // DO_CHECKS

	VkDependencyInfo dependency{ VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
    dependency.imageMemoryBarrierCount	= static_cast<Uint32>(m_textureBarriers.size());
    dependency.pImageMemoryBarriers		= m_textureBarriers.data();
    dependency.bufferMemoryBarrierCount = static_cast<Uint32>(m_bufferBarriers.size());
    dependency.pBufferMemoryBarriers	= m_bufferBarriers.data();

	return dependency;
}

void RHIDependency::ValidateBarriers() const
{
	for (SizeType idx = 0; idx < m_textureBarriers.size(); ++idx)
	{
		const VkImageMemoryBarrier2& textureBarrier = m_textureBarriers[idx];
		SPT_CHECK_MSG(textureBarrier.oldLayout != priv::g_invalidLayoutValue, "Didn't provide old layout for texture with untracked layout (idx = {0})", idx);
		SPT_CHECK(textureBarrier.newLayout != priv::g_invalidLayoutValue); // Should never happen
	}
}

} // spt::vulkan
