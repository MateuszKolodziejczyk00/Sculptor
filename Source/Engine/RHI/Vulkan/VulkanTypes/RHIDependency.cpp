#include "Vulkan/VulkanTypes/RHIDependency.h"
#include "Vulkan/LayoutsManager.h"
#include "Vulkan/VulkanTypes/RHITexture.h"
#include "Vulkan/VulkanTypes/RHICommandBuffer.h"
#include "Vulkan/VulkanRHIUtils.h"
#include "Vulkan/VulkanRHI.h"
#include "Vulkan/VulkanTypes/RHIEvent.h"
#include "Vulkan/VulkanTypes/RHIBuffer.h"

namespace spt::vulkan
{

namespace priv
{

constexpr VkImageLayout g_uninitializedLayoutValue = VK_IMAGE_LAYOUT_MAX_ENUM;

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
	case VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL:
		outStage		= VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
		outAccessFlags	= VK_ACCESS_2_SHADER_READ_BIT;

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

static VkAccessFlags2 GetVulkanAccessFlags(const rhi::BarrierTextureTransitionDefinition& transitionTarget)
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

static VkAccessFlags2 GetVulkanBufferAccessFlags(rhi::EAccessType access, rhi::EPipelineStage stage)
{
	VkAccessFlags2 result = 0;

	if (lib::HasAnyFlag(access, rhi::EAccessType::Read))
	{
		if (lib::HasAnyFlag(stage, lib::Flags(rhi::EPipelineStage::None, rhi::EPipelineStage::TopOfPipe, rhi::EPipelineStage::BottomOfPipe)))
		{
			lib::AddFlag(result, VK_ACCESS_2_MEMORY_READ_BIT);
		}

		if (lib::HasAnyFlag(stage, rhi::EPipelineStage::DrawIndirect))
		{
			lib::AddFlag(result, VK_ACCESS_2_MEMORY_READ_BIT);
			lib::AddFlag(result, VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT_KHR);
		}
		
		if (lib::HasAnyFlag(stage, lib::Flags(rhi::EPipelineStage::VertexShader, rhi::EPipelineStage::FragmentShader, rhi::EPipelineStage::ComputeShader)))
		{
			lib::AddFlag(result, VK_ACCESS_2_SHADER_READ_BIT);
		}

		if (lib::HasAnyFlag(stage, lib::Flags(rhi::EPipelineStage::Transfer, rhi::EPipelineStage::Copy)))
		{
			lib::AddFlag(result, VK_ACCESS_2_TRANSFER_READ_BIT);
		}
		
		if (lib::HasAnyFlag(stage, rhi::EPipelineStage::IndexInput))
		{
			lib::AddFlag(result, VK_ACCESS_2_INDEX_READ_BIT_KHR);
		}

		if (lib::HasAnyFlag(stage, rhi::EPipelineStage::Host))
		{
			lib::AddFlag(result, VK_ACCESS_2_HOST_READ_BIT_KHR);
		}
	}
	if (lib::HasAnyFlag(access, rhi::EAccessType::Write))
	{
		if (lib::HasAnyFlag(stage, lib::Flags(rhi::EPipelineStage::None, rhi::EPipelineStage::TopOfPipe, rhi::EPipelineStage::BottomOfPipe)))
		{
			lib::AddFlag(result, VK_ACCESS_2_MEMORY_WRITE_BIT);
		}

		if (lib::HasAnyFlag(stage, lib::Flags(rhi::EPipelineStage::VertexShader, rhi::EPipelineStage::FragmentShader, rhi::EPipelineStage::ComputeShader)))
		{
			lib::AddFlag(result, VK_ACCESS_2_SHADER_WRITE_BIT);
		}

		if (lib::HasAnyFlag(stage, lib::Flags(rhi::EPipelineStage::Transfer, rhi::EPipelineStage::Copy)))
		{
			lib::AddFlag(result, VK_ACCESS_2_TRANSFER_WRITE_BIT);
		}

		if (lib::HasAnyFlag(stage, rhi::EPipelineStage::Host))
		{
			lib::AddFlag(result, VK_ACCESS_2_HOST_WRITE_BIT);
		}
	}

	return result;
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
	barrier.oldLayout							= priv::g_uninitializedLayoutValue;
    barrier.newLayout							= priv::g_uninitializedLayoutValue;
    barrier.srcQueueFamilyIndex					= VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex					= VK_QUEUE_FAMILY_IGNORED;

	m_textureBarriers.push_back(barrier);

	return m_textureBarriers.size() - 1;
}

void RHIDependency::SetLayoutTransition(SizeType textureBarrierIdx, const rhi::BarrierTextureTransitionDefinition& transitionTarget)
{
	SPT_CHECK(textureBarrierIdx < m_textureBarriers.size());

	VkImageMemoryBarrier2& barrier = m_textureBarriers[textureBarrierIdx];
    barrier.dstStageMask	= RHIToVulkan::GetStageFlags(transitionTarget.stage);
    barrier.dstAccessMask	= priv::GetVulkanAccessFlags(transitionTarget);
    barrier.newLayout		= RHIToVulkan::GetImageLayout(transitionTarget.layout);
}

void RHIDependency::SetLayoutTransition(SizeType textureBarrierIdx, const rhi::BarrierTextureTransitionDefinition& transitionSource, const rhi::BarrierTextureTransitionDefinition& transitionTarget)
{
	SPT_CHECK(textureBarrierIdx < m_textureBarriers.size());

	VkImageMemoryBarrier2& barrier = m_textureBarriers[textureBarrierIdx];

	if (transitionSource.layout != rhi::ETextureLayout::Auto)
	{
		barrier.srcStageMask = RHIToVulkan::GetStageFlags(transitionSource.stage);
		barrier.srcAccessMask = priv::GetVulkanAccessFlags(transitionSource);
		barrier.oldLayout = RHIToVulkan::GetImageLayout(transitionSource.layout);
	}

    barrier.dstStageMask	= RHIToVulkan::GetStageFlags(transitionTarget.stage);
    barrier.dstAccessMask	= priv::GetVulkanAccessFlags(transitionTarget);
    barrier.newLayout		= RHIToVulkan::GetImageLayout(transitionTarget.layout);
}

SizeType RHIDependency::AddBufferDependency(const RHIBuffer& buffer, SizeType offset, SizeType size)
{
	SPT_PROFILER_FUNCTION();
	
	VkBufferMemoryBarrier2& barrier = m_bufferBarriers.emplace_back(VkBufferMemoryBarrier2{ VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2 });
    barrier.srcStageMask		= VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    barrier.srcAccessMask		= 0;
    barrier.dstStageMask		= VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    barrier.dstAccessMask		=  0;
	barrier.buffer				= buffer.GetHandle();
	barrier.offset				= offset;
	barrier.size				= size;
    barrier.srcQueueFamilyIndex	= VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex	= VK_QUEUE_FAMILY_IGNORED;

	return m_bufferBarriers.size() - 1;
}

void RHIDependency::SetBufferDependencyStages(SizeType bufferIdx, rhi::EPipelineStage destStage, rhi::EAccessType destAccess)
{
	SPT_CHECK(bufferIdx < m_bufferBarriers.size());
	
	VkBufferMemoryBarrier2& barrier = m_bufferBarriers[bufferIdx];

	barrier.dstStageMask	= RHIToVulkan::GetStageFlags(destStage);
	barrier.dstAccessMask	= priv::GetVulkanBufferAccessFlags(destAccess, destStage);
}

void RHIDependency::SetBufferDependencyStages(SizeType bufferIdx, rhi::EPipelineStage sourceStage, rhi::EAccessType sourceAccess, rhi::EPipelineStage destStage, rhi::EAccessType destAccess)
{
	SPT_CHECK(bufferIdx < m_bufferBarriers.size());
	
	VkBufferMemoryBarrier2& barrier = m_bufferBarriers[bufferIdx];

	barrier.srcStageMask	= RHIToVulkan::GetStageFlags(sourceStage);
	barrier.srcAccessMask	= priv::GetVulkanBufferAccessFlags(sourceAccess, sourceStage);
	barrier.dstStageMask	= RHIToVulkan::GetStageFlags(destStage);
	barrier.dstAccessMask	= priv::GetVulkanBufferAccessFlags(destAccess, destStage);
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

	ReleaseTexturesWriteAccess(cmdBuffer);
}

void RHIDependency::WaitEvent(const RHICommandBuffer& cmdBuffer, const RHIEvent& event)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(event.IsValid());

	AcquireTexturesWriteAccess(cmdBuffer);

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

void RHIDependency::ReleaseTexturesWriteAccess(const RHICommandBuffer& cmdBuffer)
{
	SPT_PROFILER_FUNCTION();

	LayoutsManager& layoutsManager = VulkanRHI::GetLayoutsManager();

	for (SizeType idx = 0; idx < m_textureBarriers.size(); ++idx)
	{
		const VkImageMemoryBarrier2& textureBarrier = m_textureBarriers[idx];

		layoutsManager.ReleaseImageWriteAccess(cmdBuffer.GetHandle(), textureBarrier.image);
	}
}

void RHIDependency::AcquireTexturesWriteAccess(const RHICommandBuffer& cmdBuffer)
{
	SPT_PROFILER_FUNCTION();

	LayoutsManager& layoutsManager = VulkanRHI::GetLayoutsManager();

	for (SizeType idx = 0; idx < m_textureBarriers.size(); ++idx)
	{
		const VkImageMemoryBarrier2& textureBarrier = m_textureBarriers[idx];

		layoutsManager.AcquireImageWriteAccess(cmdBuffer.GetHandle(), textureBarrier.image);
	}
}

void RHIDependency::ValidateBarriers() const
{
	for (SizeType idx = 0; idx < m_textureBarriers.size(); ++idx)
	{
		const VkImageMemoryBarrier2& textureBarrier = m_textureBarriers[idx];
		SPT_CHECK_MSG(textureBarrier.oldLayout != priv::g_uninitializedLayoutValue, "Didn't provide old layout for texture (idx = {0})", idx);
		SPT_CHECK(textureBarrier.newLayout != priv::g_uninitializedLayoutValue); // Should never happen
	}
}

} // spt::vulkan
