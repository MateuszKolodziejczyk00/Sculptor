#include "Vulkan/VulkanTypes/RHIDependency.h"
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
		if (lib::HasAnyFlag(stage, lib::Flags(rhi::EPipelineStage::TopOfPipe, rhi::EPipelineStage::BottomOfPipe)) || stage == rhi::EPipelineStage::None)
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

		if (lib::HasAnyFlag(stage, rhi::EPipelineStage::RayTracingShader))
		{
			lib::AddFlag(result, VK_ACCESS_2_SHADER_READ_BIT);
			lib::AddFlag(result, VK_ACCESS_2_MEMORY_READ_BIT);
		}
	}
	if (lib::HasAnyFlag(access, rhi::EAccessType::Write))
	{
		if (lib::HasAnyFlag(stage, lib::Flags(rhi::EPipelineStage::TopOfPipe, rhi::EPipelineStage::BottomOfPipe)) || stage == rhi::EPipelineStage::None)
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
		
		if (lib::HasAnyFlag(stage, rhi::EPipelineStage::ASBuild))
		{
			lib::AddFlag(result, VK_ACCESS_2_MEMORY_WRITE_BIT);
			lib::AddFlag(result, VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR);
		}
		
		if (lib::HasAnyFlag(stage, rhi::EPipelineStage::RayTracingShader))
		{
			lib::AddFlag(result, VK_ACCESS_2_MEMORY_WRITE_BIT);
			lib::AddFlag(result, VK_ACCESS_2_SHADER_WRITE_BIT);
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
{
	m_memoryBarrier = VkMemoryBarrier2{ VK_STRUCTURE_TYPE_MEMORY_BARRIER_2 };
	m_memoryBarrier.srcStageMask  = VK_PIPELINE_STAGE_2_NONE;
	m_memoryBarrier.srcAccessMask = VK_ACCESS_2_NONE;
	m_memoryBarrier.dstStageMask  = VK_PIPELINE_STAGE_2_NONE;
	m_memoryBarrier.dstAccessMask = VK_ACCESS_2_NONE;
}

Bool RHIDependency::IsEmpty() const
{
	return m_textureBarriers.empty() && m_bufferBarriers.empty();
}

SizeType RHIDependency::AddTextureDependency(const RHITexture& texture, const rhi::TextureSubresourceRange& subresourceRange)
{
	const VkImageAspectFlags aspect = RHIToVulkan::GetAspectFlags(subresourceRange.aspect == rhi::ETextureAspect::Auto
																  ? GetFullAspectForFormat(texture.GetDefinition().format)
																  : subresourceRange.aspect);

	VkImageMemoryBarrier2 barrier{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
	barrier.image                           = texture.GetHandle();
	barrier.subresourceRange.aspectMask     = aspect;
	barrier.subresourceRange.baseMipLevel   = subresourceRange.baseMipLevel;
	barrier.subresourceRange.levelCount     = subresourceRange.mipLevelsNum;
	barrier.subresourceRange.baseArrayLayer = subresourceRange.baseArrayLayer;
	barrier.subresourceRange.layerCount     = subresourceRange.arrayLayersNum;
	barrier.srcStageMask                    = VK_PIPELINE_STAGE_2_NONE;
	barrier.srcAccessMask                   = VK_ACCESS_2_NONE;
	barrier.dstStageMask                    = VK_PIPELINE_STAGE_2_NONE;
	barrier.dstAccessMask                   = VK_ACCESS_2_NONE;
	barrier.oldLayout                       = VK_IMAGE_LAYOUT_GENERAL;
	barrier.newLayout                       = VK_IMAGE_LAYOUT_GENERAL;
	barrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
	
	m_textureBarriers.push_back(barrier);

	return m_textureBarriers.size() - 1;
}

void RHIDependency::SetLayoutTransition(SizeType textureBarrierIdx, const rhi::BarrierTextureTransitionDefinition& transitionTarget)
{
	SPT_CHECK(textureBarrierIdx < m_textureBarriers.size());
	SetLayoutTransition(textureBarrierIdx, rhi::TextureTransition::Generic, transitionTarget);
}

void RHIDependency::SetLayoutTransition(SizeType textureBarrierIdx, const rhi::BarrierTextureTransitionDefinition& transitionSource, const rhi::BarrierTextureTransitionDefinition& transitionTarget)
{
	SPT_CHECK(textureBarrierIdx < m_textureBarriers.size());

	VkImageMemoryBarrier2& barrier = m_textureBarriers[textureBarrierIdx];

	barrier.srcStageMask  = RHIToVulkan::GetStageFlags(transitionSource.stage);
	barrier.srcAccessMask = priv::GetVulkanAccessFlags(transitionSource);
	barrier.oldLayout     = RHIToVulkan::GetImageLayout(transitionSource.layout);

	barrier.dstStageMask  = RHIToVulkan::GetStageFlags(transitionTarget.stage);
	barrier.dstAccessMask = priv::GetVulkanAccessFlags(transitionTarget);
	barrier.newLayout     = RHIToVulkan::GetImageLayout(transitionTarget.layout);
}

SizeType RHIDependency::AddBufferDependency(const RHIBuffer& buffer, SizeType offset, SizeType size)
{
	VkBufferMemoryBarrier2& barrier = m_bufferBarriers.emplace_back(VkBufferMemoryBarrier2{ VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2 });
    barrier.srcStageMask            = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    barrier.srcAccessMask           = 0;
    barrier.dstStageMask            = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    barrier.dstAccessMask           =  0;
	barrier.buffer                  = buffer.GetHandle();
	barrier.offset                  = offset;
	barrier.size                    = size;
    barrier.srcQueueFamilyIndex     = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex     = VK_QUEUE_FAMILY_IGNORED;

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

	barrier.srcStageMask  = RHIToVulkan::GetStageFlags(sourceStage);
	barrier.srcAccessMask = priv::GetVulkanBufferAccessFlags(sourceAccess, sourceStage);
	barrier.dstStageMask  = RHIToVulkan::GetStageFlags(destStage);
	barrier.dstAccessMask = priv::GetVulkanBufferAccessFlags(destAccess, destStage);
}

void RHIDependency::StageBarrier(rhi::EPipelineStage sourceStage, rhi::EAccessType sourceAccess, rhi::EPipelineStage destStage, rhi::EAccessType destAccess)
{
	m_memoryBarrier.srcStageMask  |= RHIToVulkan::GetStageFlags(sourceStage);
	m_memoryBarrier.srcAccessMask |= priv::GetVulkanBufferAccessFlags(sourceAccess, sourceStage);
	m_memoryBarrier.dstStageMask  |= RHIToVulkan::GetStageFlags(destStage);
	m_memoryBarrier.dstAccessMask |= priv::GetVulkanBufferAccessFlags(destAccess, destStage);
}

void RHIDependency::ExecuteBarrier(const RHICommandBuffer& cmdBuffer) const
{
	const VkDependencyInfo dependencyInfo = GetDependencyInfo();
	vkCmdPipelineBarrier2(cmdBuffer.GetHandle(), &dependencyInfo);
}

void RHIDependency::SetEvent(const RHICommandBuffer& cmdBuffer, const RHIEvent& event)
{
	SPT_CHECK(event.IsValid());

	const VkDependencyInfo dependencyInfo = GetDependencyInfo();
	vkCmdSetEvent2(cmdBuffer.GetHandle(), event.GetHandle(), &dependencyInfo);
}

void RHIDependency::WaitEvent(const RHICommandBuffer& cmdBuffer, const RHIEvent& event)
{
	SPT_CHECK(event.IsValid());

	const VkDependencyInfo dependencyInfo = GetDependencyInfo();

	const VkEvent eventHandle = event.GetHandle();
	vkCmdWaitEvents2(cmdBuffer, 1, &eventHandle, &dependencyInfo);
}

VkDependencyInfo RHIDependency::GetDependencyInfo() const
{
#if DO_CHECKS
	ValidateBarriers();
#endif // DO_CHECKS

	VkDependencyInfo dependency{ VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
	dependency.imageMemoryBarrierCount  = static_cast<Uint32>(m_textureBarriers.size());
	dependency.pImageMemoryBarriers	    = m_textureBarriers.data();
	dependency.bufferMemoryBarrierCount = static_cast<Uint32>(m_bufferBarriers.size());
	dependency.pBufferMemoryBarriers    = m_bufferBarriers.data();
	dependency.memoryBarrierCount       = 1u;
	dependency.pMemoryBarriers          = &m_memoryBarrier;

	return dependency;
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
