#include "RHICommandBuffer.h"
#include "Vulkan/VulkanRHI.h"
#include "Vulkan/CommandPool/RHICommandPoolsManager.h"
#include "Vulkan/LayoutsManager.h"
#include "Vulkan/VulkanRHIUtils.h"
#include "RHITexture.h"
#include "RHIPipeline.h"
#include "RHIDescriptorSet.h"

namespace spt::vulkan
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// Helpers =======================================================================================

namespace priv
{

VkCommandBufferUsageFlags GetVulkanCommandBufferUsageFlags(rhi::ECommandBufferBeginFlags rhiFlags)
{
	VkCommandBufferUsageFlags usage = 0;

	if (lib::HasAnyFlag(rhiFlags, rhi::ECommandBufferBeginFlags::OneTimeSubmit))
	{
		lib::AddFlag(usage, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
	}
	if (lib::HasAnyFlag(rhiFlags, rhi::ECommandBufferBeginFlags::ContinueRendering))
	{
		lib::AddFlag(usage, VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT);
	}

	return usage;
}

} // priv

//////////////////////////////////////////////////////////////////////////////////////////////////
// RHICommandBuffer ==============================================================================

RHICommandBuffer::RHICommandBuffer()
	: m_cmdBufferHandle(VK_NULL_HANDLE)
	, m_queueType(rhi::ECommandBufferQueueType::Graphics)
	, m_cmdBufferType(rhi::ECommandBufferType::Primary)
{ }

void RHICommandBuffer::InitializeRHI(const rhi::CommandBufferDefinition& bufferDefinition)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(!IsValid());

	m_cmdBufferHandle	= VulkanRHI::GetCommandPoolsManager().AcquireCommandBuffer(bufferDefinition, m_acquireInfo);
	m_queueType			= bufferDefinition.queueType;
	m_cmdBufferType		= bufferDefinition.cmdBufferType;
}

void RHICommandBuffer::ReleaseRHI()
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(!!IsValid());

	VulkanRHI::GetCommandPoolsManager().ReleaseCommandBuffer(m_acquireInfo, m_cmdBufferHandle);

	m_cmdBufferHandle = VK_NULL_HANDLE;
	m_acquireInfo.Reset();
	m_name.Reset();
}

Bool RHICommandBuffer::IsValid() const
{
	return m_cmdBufferHandle != VK_NULL_HANDLE;
}

VkCommandBuffer RHICommandBuffer::GetHandle() const
{
	return m_cmdBufferHandle;
}

rhi::ECommandBufferQueueType RHICommandBuffer::GetQueueType() const
{
	return m_queueType;
}

void RHICommandBuffer::StartRecording(const rhi::CommandBufferUsageDefinition& usageDefinition)
{
	SPT_CHECK(IsValid());

	VkCommandBufferBeginInfo beginInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	beginInfo.flags = priv::GetVulkanCommandBufferUsageFlags(usageDefinition.beginFlags);

	vkBeginCommandBuffer(m_cmdBufferHandle, &beginInfo);

	VulkanRHI::GetLayoutsManager().RegisterRecordingCommandBuffer(m_cmdBufferHandle);
}

void RHICommandBuffer::StopRecording()
{
	VulkanRHI::GetLayoutsManager().UnregisterRecordingCommnadBuffer(m_cmdBufferHandle);

	vkEndCommandBuffer(m_cmdBufferHandle);

	VulkanRHI::GetCommandPoolsManager().ReleasePool(m_acquireInfo);
}

void RHICommandBuffer::SetName(const lib::HashedString& name)
{
	m_name.Set(name, reinterpret_cast<Uint64>(m_cmdBufferHandle), VK_OBJECT_TYPE_COMMAND_BUFFER);
}

const lib::HashedString& RHICommandBuffer::GetName() const
{
	return m_name.Get();
}

void RHICommandBuffer::BeginRendering(const rhi::RenderingDefinition& renderingDefinition)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(IsValid());
	
	const auto CreateAttachmentInfo = [this](const rhi::RHIRenderTargetDefinition& renderTarget, Bool isColor)
	{
		LayoutsManager& layoutsManager = VulkanRHI::GetLayoutsManager();

		VkRenderingAttachmentInfo attachmentInfo{ VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };

		const RHITextureView& textureView = renderTarget.textureView;
		SPT_CHECK(textureView.IsValid() && textureView.GetTexture());
		const RHITexture& texture = *textureView.GetTexture();

		attachmentInfo.imageView	= textureView.GetHandle();
		attachmentInfo.imageLayout	= layoutsManager.GetSubresourcesSharedLayout(m_cmdBufferHandle,
																				texture.GetHandle(),
																				textureView.GetSubresourceRange());

		if (renderTarget.resolveTextureView.IsValid())
		{
			const RHITextureView& resolveTextureView = renderTarget.resolveTextureView;
			SPT_CHECK(!!resolveTextureView.GetTexture());
			const RHITexture& resolveTexture = *resolveTextureView.GetTexture();

			attachmentInfo.resolveMode			= RHIToVulkan::GetResolveMode(renderTarget.resolveMode);
			attachmentInfo.resolveImageView		= resolveTextureView.GetHandle();
			attachmentInfo.resolveImageLayout	= layoutsManager.GetSubresourcesSharedLayout(m_cmdBufferHandle,
																							 resolveTexture.GetHandle(),
																							 resolveTextureView.GetSubresourceRange());
		}

		attachmentInfo.loadOp	= RHIToVulkan::GetLoadOp(renderTarget.loadOperation);
		attachmentInfo.storeOp	= RHIToVulkan::GetStoreOp(renderTarget.storeOperation);
		if (isColor)
		{
			memcpy(&attachmentInfo.clearValue, &renderTarget.clearColor, sizeof(VkClearValue));
		}
		else
		{
			attachmentInfo.clearValue.depthStencil.depth	= renderTarget.clearColor.asDepthStencil.depth;
			attachmentInfo.clearValue.depthStencil.stencil	= renderTarget.clearColor.asDepthStencil.stencil;
		}

		return attachmentInfo;
	};

	const auto CreateColorAttachmentInfo = [&CreateAttachmentInfo](const rhi::RHIRenderTargetDefinition& renderTarget)
	{
		return CreateAttachmentInfo(renderTarget, true);
	};

	const auto CreateDepthStencilAttachmentInfo = [&CreateAttachmentInfo](const rhi::RHIRenderTargetDefinition& renderTarget)
	{
		return CreateAttachmentInfo(renderTarget, false);
	};

	lib::DynamicArray<VkRenderingAttachmentInfo> colorAttachments;
	colorAttachments.reserve(renderingDefinition.colorRTs.size());

	std::transform(renderingDefinition.colorRTs.cbegin(), renderingDefinition.colorRTs.end(),
				   std::back_inserter(colorAttachments),
				   CreateColorAttachmentInfo);

	const Bool hasDepthAttachment = renderingDefinition.depthRT.textureView.IsValid();
	VkRenderingAttachmentInfo depthAttachmentInfo{};
	if (hasDepthAttachment)
	{
		depthAttachmentInfo = CreateDepthStencilAttachmentInfo(renderingDefinition.depthRT);
	}

	const Bool hasStencilAttachment = renderingDefinition.stencilRT.textureView.IsValid();
	VkRenderingAttachmentInfo stencilAttachmentInfo{};
	if (hasStencilAttachment)
	{
		stencilAttachmentInfo = CreateDepthStencilAttachmentInfo(renderingDefinition.stencilRT);
	}

	const math::Vector2i areaOffset = renderingDefinition.renderAreaOffset;
	const math::Vector2u areaExtent = renderingDefinition.renderAreaExtent;

	VkRenderingInfo renderingInfo{ VK_STRUCTURE_TYPE_RENDERING_INFO };
    renderingInfo.flags					= RHIToVulkan::GetRenderingFlags(renderingDefinition.renderingFlags);
    renderingInfo.renderArea.offset		= VkOffset2D{ areaOffset.x(), areaOffset.y() };
    renderingInfo.renderArea.extent		= VkExtent2D{ areaExtent.x(), areaExtent.y() };
    renderingInfo.layerCount			= 1;
    renderingInfo.viewMask				= 0;
    renderingInfo.colorAttachmentCount	= static_cast<Uint32>(colorAttachments.size());
    renderingInfo.pColorAttachments		= colorAttachments.data();
    renderingInfo.pDepthAttachment		= hasDepthAttachment ? &depthAttachmentInfo : nullptr;
    renderingInfo.pStencilAttachment	= hasStencilAttachment ? &stencilAttachmentInfo : nullptr;

	vkCmdBeginRendering(m_cmdBufferHandle, &renderingInfo);
}

void RHICommandBuffer::EndRendering()
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(IsValid());

	vkCmdEndRendering(m_cmdBufferHandle);
}

void RHICommandBuffer::BindGfxPipeline(const RHIPipeline& pipeline)
{
	BindPipelineImpl(VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
}

void RHICommandBuffer::BindGfxDescriptorSet(const RHIPipeline& pipeline, const RHIDescriptorSet& ds, Uint32 dsIdx, const lib::DynamicArray<Uint32>& dynamicOffsets)
{
	BindDescriptorSetImpl(VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline, ds, dsIdx, dynamicOffsets);
}

void RHICommandBuffer::BindComputePipeline(const RHIPipeline& pipeline)
{
	BindPipelineImpl(VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
}

void RHICommandBuffer::BindComputeDescriptorSet(const RHIPipeline& pipeline, const RHIDescriptorSet& ds, Uint32 dsIdx, const lib::DynamicArray<Uint32>& dynamicOffsets)
{
	BindDescriptorSetImpl(VK_PIPELINE_BIND_POINT_COMPUTE, pipeline, ds, dsIdx, dynamicOffsets);
}

void RHICommandBuffer::Dispatch(const math::Vector3u& groupCount)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(IsValid());
	SPT_CHECK(groupCount.x() > 0 && groupCount.y() > 0 && groupCount.z() > 0);

	vkCmdDispatch(m_cmdBufferHandle, groupCount.x(), groupCount.y(), groupCount.z());
}

void RHICommandBuffer::CopyTexture(const RHITexture& source, const rhi::TextureCopyRange& sourceRange, const RHITexture& target, const rhi::TextureCopyRange& targetRange, const math::Vector3u& extent)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(IsValid());
	SPT_CHECK(source.IsValid());
	SPT_CHECK(target.IsValid());

	const LayoutsManager& layoutsManager = VulkanRHI::GetLayoutsManager();

	const VkImage sourceImage = source.GetHandle();
	const VkImageLayout sourceLayout = layoutsManager.GetSubresourcesSharedLayout(m_cmdBufferHandle, sourceImage, ImageSubresourceRange(sourceRange.mipLevel, 1, sourceRange.baseArrayLayer, sourceRange.arrayLayersNum));

	const VkImage targetImage = target.GetHandle();
	const VkImageLayout targetLayout = layoutsManager.GetSubresourcesSharedLayout(m_cmdBufferHandle, targetImage, ImageSubresourceRange(targetRange.mipLevel, 1, targetRange.baseArrayLayer, targetRange.arrayLayersNum));

	const auto getArrayLayersNum = [](const rhi::TextureCopyRange& range, const RHITexture& texture) -> Uint32
	{
		if (range.arrayLayersNum == rhi::constants::allRemainingArrayLayers)
		{
			SPT_CHECK(texture.GetDefinition().arrayLayers > range.baseArrayLayer);
			return texture.GetDefinition().arrayLayers - range.baseArrayLayer;
		}
		{
			const Uint32 arrayLayersEnd = range.baseArrayLayer + range.arrayLayersNum;
			SPT_CHECK(texture.GetDefinition().arrayLayers >= arrayLayersEnd); // check if array layers range is in texture range

			return range.arrayLayersNum;
		}
	};

	VkImageCopy2 copyRegion{ VK_STRUCTURE_TYPE_IMAGE_COPY_2 };
    copyRegion.srcSubresource.aspectMask		= RHIToVulkan::GetAspectFlags(sourceRange.aspect);
    copyRegion.srcSubresource.mipLevel			= sourceRange.mipLevel;
    copyRegion.srcSubresource.baseArrayLayer	= sourceRange.baseArrayLayer;
    copyRegion.srcSubresource.layerCount		= getArrayLayersNum(sourceRange, source);
	copyRegion.srcOffset						= VkOffset3D{ .x = sourceRange.offset.x(), .y = sourceRange.offset.y(), .z = sourceRange.offset.z() };
    copyRegion.dstSubresource.aspectMask		= RHIToVulkan::GetAspectFlags(targetRange.aspect);
    copyRegion.dstSubresource.mipLevel			= targetRange.mipLevel;
    copyRegion.dstSubresource.baseArrayLayer	= targetRange.baseArrayLayer;
    copyRegion.dstSubresource.layerCount		= getArrayLayersNum(targetRange, target);
	copyRegion.dstOffset						= VkOffset3D{ .x = targetRange.offset.x(), .y = targetRange.offset.y(), .z = targetRange.offset.z() };
    copyRegion.extent							= VkExtent3D{ .width = extent.x(), .height = extent.y(), .depth = extent.z() };
 
	const VkCopyImageInfo2 copyInfo
	{
		.sType = VK_STRUCTURE_TYPE_COPY_IMAGE_INFO_2,
		.srcImage = sourceImage,
		.srcImageLayout = sourceLayout,
		.dstImage = targetImage,
		.dstImageLayout = targetLayout,
		.regionCount = 1,
		.pRegions = &copyRegion
	};

	vkCmdCopyImage2(m_cmdBufferHandle, &copyInfo);
}

void RHICommandBuffer::BindPipelineImpl(VkPipelineBindPoint bindPoint, const RHIPipeline& pipeline)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(IsValid());
	SPT_CHECK(pipeline.IsValid());

	vkCmdBindPipeline(m_cmdBufferHandle, bindPoint, pipeline.GetHandle());
}

void RHICommandBuffer::BindDescriptorSetImpl(VkPipelineBindPoint bindPoint, const RHIPipeline& pipeline, const RHIDescriptorSet& ds, Uint32 dsIdx, const lib::DynamicArray<Uint32>& dynamicOffsets)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(IsValid());
	SPT_CHECK(ds.IsValid());
	SPT_CHECK(pipeline.IsValid());
	SPT_CHECK(dsIdx != idxNone<Uint32>);

	const PipelineLayout& layout = pipeline.GetPipelineLayout();
	VkDescriptorSet dsHandle = ds.GetHandle();

	vkCmdBindDescriptorSets(m_cmdBufferHandle, bindPoint, layout.GetHandle(), dsIdx, 1, &dsHandle, static_cast<Uint32>(dynamicOffsets.size()), dynamicOffsets.data());
}

} // spt::vulkan
