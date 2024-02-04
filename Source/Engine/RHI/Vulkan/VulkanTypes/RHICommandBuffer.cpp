#include "RHICommandBuffer.h"
#include "Vulkan/VulkanRHI.h"
#include "Vulkan/CommandPool/RHICommandPoolsManager.h"
#include "Vulkan/LayoutsManager.h"
#include "Vulkan/VulkanRHIUtils.h"
#include "RHITexture.h"
#include "RHIBuffer.h"
#include "RHIPipeline.h"
#include "RHIDescriptorSet.h"
#include "RHIRenderContext.h"
#include "RHIAccelerationStructure.h"
#include "RHIShaderBindingTable.h"
#include "RHIQueryPool.h"
#include "Vulkan/VulkanUtils.h"

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
	, m_queueType(rhi::EDeviceCommandQueueType::Graphics)
	, m_cmdBufferType(rhi::ECommandBufferType::Primary)
{ }

void RHICommandBuffer::InitializeRHI(RHIRenderContext& renderContext, const rhi::CommandBufferDefinition& bufferDefinition)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(!IsValid());

	m_cmdBufferHandle	= renderContext.AcquireCommandBuffer(bufferDefinition);
	m_queueType			= bufferDefinition.queueType;
	m_cmdBufferType		= bufferDefinition.cmdBufferType;
}

void RHICommandBuffer::ReleaseRHI()
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(!!IsValid());

	m_name.Reset(reinterpret_cast<Uint64>(m_cmdBufferHandle), VK_OBJECT_TYPE_COMMAND_BUFFER);
	m_cmdBufferHandle = VK_NULL_HANDLE;
}

Bool RHICommandBuffer::IsValid() const
{
	return m_cmdBufferHandle != VK_NULL_HANDLE;
}

rhi::EDeviceCommandQueueType RHICommandBuffer::GetQueueType() const
{
	return m_queueType;
}

void RHICommandBuffer::StartRecording(const rhi::CommandBufferUsageDefinition& usageDefinition)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(IsValid());

	VkCommandBufferInheritanceInfo inheritanceInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO };
	VulkanStructsLinkedList inheritanceInfoLinkedData(inheritanceInfo);

	VkCommandBufferBeginInfo beginInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	beginInfo.flags            = priv::GetVulkanCommandBufferUsageFlags(usageDefinition.beginFlags);
	beginInfo.pInheritanceInfo = &inheritanceInfo;

	VkCommandBufferInheritanceRenderingInfo vkInheritanceRenderingInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_RENDERING_INFO };

	lib::DynamicArray<VkFormat> colorRTFormats;

	if (usageDefinition.renderingInheritance)
	{
		const rhi::RenderingInheritanceDefinition& renderingInheritance = *usageDefinition.renderingInheritance;

		colorRTFormats.reserve(renderingInheritance.colorRTFormats.size());
		std::transform(renderingInheritance.colorRTFormats.cbegin(), renderingInheritance.colorRTFormats.cend(),
					   std::back_inserter(colorRTFormats),
					   [](rhi::EFragmentFormat format)
					   {
						   return RHIToVulkan::GetVulkanFormat(format);
					   });

		vkInheritanceRenderingInfo.flags                   = RHIToVulkan::GetRenderingFlags(renderingInheritance.flags);
		vkInheritanceRenderingInfo.colorAttachmentCount    = static_cast<Uint32>(renderingInheritance.colorRTFormats.size());
		vkInheritanceRenderingInfo.pColorAttachmentFormats = colorRTFormats.data();
		vkInheritanceRenderingInfo.depthAttachmentFormat   = RHIToVulkan::GetVulkanFormat(renderingInheritance.depthRTFormat);
		vkInheritanceRenderingInfo.stencilAttachmentFormat = RHIToVulkan::GetVulkanFormat(renderingInheritance.stencilRTFormat);
		vkInheritanceRenderingInfo.rasterizationSamples    = VK_SAMPLE_COUNT_1_BIT;

		inheritanceInfoLinkedData.Append(vkInheritanceRenderingInfo);
	}

	vkBeginCommandBuffer(m_cmdBufferHandle, &beginInfo);

	VulkanRHI::GetLayoutsManager().RegisterRecordingCommandBuffer(m_cmdBufferHandle);
}

void RHICommandBuffer::StopRecording()
{
	VulkanRHI::GetLayoutsManager().UnregisterRecordingCommnadBuffer(m_cmdBufferHandle);

	vkEndCommandBuffer(m_cmdBufferHandle);
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

void RHICommandBuffer::SetViewport(const math::AlignedBox2f& renderingViewport, Real32 minDepth, Real32 maxDepth)
{
	SPT_PROFILER_FUNCTION();

	VkViewport viewport{};
	viewport.x			= renderingViewport.min().x();
	viewport.y			= renderingViewport.min().y();
    viewport.width		= renderingViewport.max().x();
    viewport.height		= renderingViewport.max().y();
    viewport.minDepth	= minDepth;
    viewport.maxDepth	= maxDepth;

	vkCmdSetViewport(m_cmdBufferHandle, 0, 1, &viewport);
}

void RHICommandBuffer::SetScissor(const math::AlignedBox2u& renderingScissor)
{
	SPT_PROFILER_FUNCTION();

	VkRect2D scissor{};
	scissor.offset.x		= renderingScissor.min().x();
	scissor.offset.y		= renderingScissor.min().y();
	scissor.extent.width	= renderingScissor.max().x();
	scissor.extent.height	= renderingScissor.max().y();
	
	vkCmdSetScissor(m_cmdBufferHandle, 0, 1, &scissor);
}

void RHICommandBuffer::DrawIndirectCount(const RHIBuffer& drawsBuffer, Uint64 drawsOffset, Uint32 drawsStride, const RHIBuffer& countBuffer, Uint64 countOffset, Uint32 maxDrawsCount)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(IsValid());
	SPT_CHECK(drawsBuffer.IsValid());
	SPT_CHECK(drawsOffset + drawsStride * maxDrawsCount <= drawsBuffer.GetSize());
	SPT_CHECK(countBuffer.IsValid());
	SPT_CHECK(countOffset + sizeof(Uint32) <= drawsBuffer.GetSize());

	vkCmdDrawIndirectCount(m_cmdBufferHandle, drawsBuffer.GetHandle(), drawsOffset, countBuffer.GetHandle(), countOffset, maxDrawsCount, drawsStride);
}

void RHICommandBuffer::DrawIndirect(const RHIBuffer& drawsBuffer, Uint64 drawsOffset, Uint32 drawsStride, Uint32 drawsCount)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(IsValid());
	SPT_CHECK(drawsBuffer.IsValid());
	SPT_CHECK(drawsOffset + drawsStride * drawsCount <= drawsBuffer.GetSize());

	vkCmdDrawIndirect(m_cmdBufferHandle, drawsBuffer.GetHandle(), drawsOffset, drawsCount, drawsStride);
}

void RHICommandBuffer::DrawInstances(Uint32 verticesNum, Uint32 instancesNum, Uint32 firstVertex, Uint32 firstInstance)
{
	SPT_PROFILER_FUNCTION();

	vkCmdDraw(m_cmdBufferHandle, verticesNum, instancesNum, firstVertex, firstInstance);
}

void RHICommandBuffer::BindGfxPipeline(const RHIPipeline& pipeline)
{
	BindPipelineImpl(VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
}

void RHICommandBuffer::BindGfxDescriptorSet(const RHIPipeline& pipeline, const RHIDescriptorSet& ds, Uint32 dsIdx, const Uint32* dynamicOffsets, Uint32 dynamicOffsetsNum)
{
	BindDescriptorSetImpl(VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline, ds, dsIdx, dynamicOffsets, dynamicOffsetsNum);
}

void RHICommandBuffer::BindComputePipeline(const RHIPipeline& pipeline)
{
	BindPipelineImpl(VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
}

void RHICommandBuffer::BindComputeDescriptorSet(const RHIPipeline& pipeline, const RHIDescriptorSet& ds, Uint32 dsIdx, const Uint32* dynamicOffsets, Uint32 dynamicOffsetsNum)
{
	BindDescriptorSetImpl(VK_PIPELINE_BIND_POINT_COMPUTE, pipeline, ds, dsIdx, dynamicOffsets, dynamicOffsetsNum);
}

void RHICommandBuffer::Dispatch(const math::Vector3u& groupCount)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(IsValid());
	SPT_CHECK(groupCount.x() > 0 && groupCount.y() > 0 && groupCount.z() > 0);

	vkCmdDispatch(m_cmdBufferHandle, groupCount.x(), groupCount.y(), groupCount.z());
}

void RHICommandBuffer::DispatchIndirect(const RHIBuffer& indirectArgsBuffer, Uint64 indirectArgsOffset)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(IsValid());
	SPT_CHECK(indirectArgsBuffer.IsValid());
	SPT_CHECK(indirectArgsOffset + sizeof(Uint32) * 3 <= indirectArgsBuffer.GetSize());

	vkCmdDispatchIndirect(m_cmdBufferHandle, indirectArgsBuffer.GetHandle(), indirectArgsOffset);
}

void RHICommandBuffer::BuildBLAS(const RHIBottomLevelAS& blas, const RHIBuffer& scratchBuffer, Uint64 scratchBufferOffset)
{
	VkAccelerationStructureGeometryKHR geometry;
	VkAccelerationStructureBuildGeometryInfoKHR buildInfo = blas.CreateBuildGeometryInfo(OUT geometry);

	BuildASImpl(blas, buildInfo, scratchBuffer, scratchBufferOffset);
}

void RHICommandBuffer::BuildTLAS(const RHITopLevelAS& tlas, const RHIBuffer& scratchBuffer, Uint64 scratchBufferOffset, const RHIBuffer& instancesBuildDataBuffer)
{
	VkAccelerationStructureGeometryKHR geometry;
	VkAccelerationStructureBuildGeometryInfoKHR buildInfo = tlas.CreateBuildGeometryInfo(instancesBuildDataBuffer, OUT geometry);

	BuildASImpl(tlas, buildInfo, scratchBuffer, scratchBufferOffset);
}

void RHICommandBuffer::BindRayTracingPipeline(const RHIPipeline& pipeline)
{
	BindPipelineImpl(VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline);
}

void RHICommandBuffer::BindRayTracingDescriptorSet(const RHIPipeline& pipeline, const RHIDescriptorSet& ds, Uint32 dsIdx, const Uint32* dynamicOffsets, Uint32 dynamicOffsetsNum)
{
	BindDescriptorSetImpl(VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline, ds, dsIdx, dynamicOffsets, dynamicOffsetsNum);
}

void RHICommandBuffer::TraceRays(const RHIShaderBindingTable& sbt, const math::Vector3u& traceCount)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(IsValid());
	SPT_CHECK(traceCount.x() > 0 && traceCount.y() > 0 && traceCount.z() > 0);

	const VkStridedDeviceAddressRegionKHR callableRegion = {};

	vkCmdTraceRaysKHR(m_cmdBufferHandle,
					  &sbt.GetRayGenRegion(),
					  &sbt.GetMissRegion(),
					  &sbt.GetClosestHitRegion(),
					  &callableRegion,
					  traceCount.x(), traceCount.y(), traceCount.z());
}

void RHICommandBuffer::BlitTexture(const RHITexture& source, Uint32 sourceMipLevel, Uint32 sourceArrayLayer, const RHITexture& dest, Uint32 destMipLevel, Uint32 destArrayLayer, rhi::ETextureAspect aspect, rhi::ESamplerFilterType filterMode)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(IsValid());
	SPT_CHECK(source.IsValid());
	SPT_CHECK(dest.IsValid());

	const math::Vector3u sourceResolution	= source.GetMipResolution(sourceMipLevel);
	const math::Vector3u destResolution		= dest.GetMipResolution(destMipLevel);

	const LayoutsManager& layoutsManager = VulkanRHI::GetLayoutsManager();
	const VkImageLayout sourceLayout	= layoutsManager.GetSubresourceLayout(m_cmdBufferHandle, source.GetHandle(), sourceMipLevel, sourceArrayLayer);
	const VkImageLayout destLayout		= layoutsManager.GetSubresourceLayout(m_cmdBufferHandle, dest.GetHandle(), destMipLevel, destArrayLayer);

	const VkImageAspectFlags vulkanAspect = RHIToVulkan::GetAspectFlags(aspect);

	VkImageBlit2 blitRegion{ VK_STRUCTURE_TYPE_IMAGE_BLIT_2 };
	blitRegion.srcSubresource = VkImageSubresourceLayers{ vulkanAspect, sourceMipLevel, sourceArrayLayer, 1 };
	math::Map<math::Vector3i>(&blitRegion.srcOffsets[0].x) = math::Vector3i::Zero();
	math::Map<math::Vector3i>(&blitRegion.srcOffsets[1].x) = sourceResolution.cast<Int32>();
    blitRegion.dstSubresource = VkImageSubresourceLayers{ vulkanAspect, destMipLevel, destArrayLayer, 1 };
	math::Map<math::Vector3i>(&blitRegion.dstOffsets[0].x) = math::Vector3i::Zero();
	math::Map<math::Vector3i>(&blitRegion.dstOffsets[1].x) = destResolution.cast<Int32>();

	VkBlitImageInfo2 blitInfo{ VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2 };
    blitInfo.srcImage		= source.GetHandle();
    blitInfo.srcImageLayout	= sourceLayout;
    blitInfo.dstImage		= dest.GetHandle();
	blitInfo.dstImageLayout	= destLayout;
    blitInfo.regionCount	= 1;
	blitInfo.pRegions		= &blitRegion;
    blitInfo.filter			= RHIToVulkan::GetSamplerFilterType(filterMode);

	vkCmdBlitImage2(m_cmdBufferHandle, &blitInfo);
}

void RHICommandBuffer::ClearTexture(const RHITexture& texture, const rhi::ClearColor& clearColor, const rhi::TextureSubresourceRange& subresourceRange)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(IsValid());
	SPT_CHECK(texture.IsValid());
	
	const LayoutsManager& layoutsManager = VulkanRHI::GetLayoutsManager();
	const VkImageLayout textureLayout	= layoutsManager.GetSubresourcesSharedLayout(m_cmdBufferHandle, texture.GetHandle(), subresourceRange);

	VkImageSubresourceRange vulkanSubresourceRange{};
    vulkanSubresourceRange.aspectMask		= RHIToVulkan::GetAspectFlags(subresourceRange.aspect);
    vulkanSubresourceRange.baseMipLevel		= subresourceRange.baseMipLevel;
    vulkanSubresourceRange.levelCount		= subresourceRange.mipLevelsNum == rhi::constants::allRemainingMips ? VK_REMAINING_MIP_LEVELS : subresourceRange.mipLevelsNum;
    vulkanSubresourceRange.baseArrayLayer	= subresourceRange.baseArrayLayer;
    vulkanSubresourceRange.layerCount		= subresourceRange.arrayLayersNum == rhi::constants::allRemainingArrayLayers ? VK_REMAINING_ARRAY_LAYERS : subresourceRange.arrayLayersNum;

	vkCmdClearColorImage(m_cmdBufferHandle, texture.GetHandle(), textureLayout, reinterpret_cast<const VkClearColorValue*>(&clearColor), 1, &vulkanSubresourceRange);
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
		else
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

	{
		SPT_PROFILER_SCOPE("vkCmdCopyImage2");
		vkCmdCopyImage2(m_cmdBufferHandle, &copyInfo);
	}
}

void RHICommandBuffer::CopyBuffer(const RHIBuffer& sourceBuffer, Uint64 sourceOffset, const RHIBuffer& destBuffer, Uint64 destOffset, Uint64 size)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(IsValid());
	SPT_CHECK(sourceBuffer.IsValid());
	SPT_CHECK(destBuffer.IsValid());
	SPT_CHECK(sourceOffset + size <= sourceBuffer.GetSize());
	SPT_CHECK(destOffset + size <= destBuffer.GetSize());

	VkBufferCopy2 copyRegion{ VK_STRUCTURE_TYPE_BUFFER_COPY_2 };
    copyRegion.srcOffset = sourceOffset;
    copyRegion.dstOffset = destOffset;
    copyRegion.size = size;

	VkCopyBufferInfo2 copyInfo{ VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2 };
    copyInfo.srcBuffer = sourceBuffer.GetHandle();
    copyInfo.dstBuffer = destBuffer.GetHandle();
    copyInfo.regionCount = 1;
    copyInfo.pRegions = &copyRegion;

	vkCmdCopyBuffer2(m_cmdBufferHandle, &copyInfo);
}

void RHICommandBuffer::FillBuffer(const RHIBuffer& buffer, Uint64 offset, Uint64 range, Uint32 data)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(IsValid());
	SPT_CHECK(buffer.IsValid());
	SPT_CHECK(offset + range <= buffer.GetSize());

	vkCmdFillBuffer(m_cmdBufferHandle, buffer.GetHandle(), offset, range, data);
}

void RHICommandBuffer::CopyBufferToTexture(const RHIBuffer& buffer, Uint64 bufferOffset, const RHITexture& texture, rhi::ETextureAspect aspect, math::Vector3u copyExtent, math::Vector3u copyOffset /*= math::Vector3u::Zero()*/, Uint32 mipLevel /*= 0*/, Uint32 arrayLayer /*= 0*/)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(IsValid());
	SPT_CHECK(buffer.IsValid());
	SPT_CHECK(texture.IsValid());

	const VkImageLayout layout = VulkanRHI::GetLayoutsManager().GetSubresourceLayout(m_cmdBufferHandle, texture.GetHandle(), mipLevel, arrayLayer);

	VkImageSubresourceLayers imageSubresource;
    imageSubresource.aspectMask		= RHIToVulkan::GetAspectFlags(aspect);
    imageSubresource.mipLevel		= mipLevel;
    imageSubresource.baseArrayLayer	= arrayLayer;
    imageSubresource.layerCount		= 1;

	VkBufferImageCopy2 copyRegion{ VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2 };
    copyRegion.bufferOffset			= bufferOffset;
    copyRegion.bufferRowLength		= 0;
    copyRegion.bufferImageHeight	= 0;
    copyRegion.imageSubresource		= imageSubresource;

	math::Map<math::Vector3i>((Int32*)&copyRegion.imageOffset) = copyOffset.cast<Int32>();
	math::Map<math::Vector3i>((Int32*)&copyRegion.imageExtent) = copyExtent.cast<Int32>();

	VkCopyBufferToImageInfo2 copyInfo{ VK_STRUCTURE_TYPE_COPY_BUFFER_TO_IMAGE_INFO_2 };
    copyInfo.srcBuffer		= buffer.GetHandle();
    copyInfo.dstImage		= texture.GetHandle();
	copyInfo.dstImageLayout = layout;
    copyInfo.regionCount	= 1;
    copyInfo.pRegions		= &copyRegion;

	vkCmdCopyBufferToImage2(m_cmdBufferHandle, &copyInfo);
}

void RHICommandBuffer::ExecuteCommands(const RHICommandBuffer& secondaryCommandBuffer)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(IsValid());
	SPT_CHECK(secondaryCommandBuffer.IsValid());

	const VkCommandBuffer secondaryCommandBufferHandle = secondaryCommandBuffer.GetHandle();
	vkCmdExecuteCommands(m_cmdBufferHandle, 1, &secondaryCommandBufferHandle);
}

void RHICommandBuffer::BeginDebugRegion(const lib::HashedString& name, const lib::Color& color)
{
#if RHI_DEBUG

	SPT_PROFILER_FUNCTION();

	SPT_CHECK(IsValid());

	const VkDebugMarkerMarkerInfoEXT markerInfo
	{
		.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_MARKER_INFO_EXT,
		.pMarkerName = name.GetData(),
		.color = { color.r, color.g, color.b, color.a }
	};

	vkCmdDebugMarkerBeginEXT(m_cmdBufferHandle, &markerInfo);

#endif // RHI_DEBUG
}

void RHICommandBuffer::EndDebugRegion()
{
#if RHI_DEBUG

	SPT_PROFILER_FUNCTION();

	SPT_CHECK(IsValid());

	vkCmdDebugMarkerEndEXT(m_cmdBufferHandle);

#endif // RHI_DEBUG
}

#if WITH_GPU_CRASH_DUMPS
void RHICommandBuffer::SetDebugCheckpoint(const void* markerPtr)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(IsValid());

	if (VulkanRHI::GetSettings().AreGPUCrashDumpsEnabled())
	{
		vkCmdSetCheckpointNV(m_cmdBufferHandle, markerPtr);
	}
}
#endif // WITH_GPU_CRASH_DUMPS

void RHICommandBuffer::ResetQueryPool(const RHIQueryPool& queryPool, Uint32 firstQueryIdx, Uint32 queryCount)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(IsValid());
	SPT_CHECK(queryPool.IsValid());

	vkCmdResetQueryPool(m_cmdBufferHandle, queryPool.GetHandle(), firstQueryIdx, queryCount);
}

void RHICommandBuffer::WriteTimestamp(const RHIQueryPool& queryPool, Uint32 queryIdx, rhi::EPipelineStage stage)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(IsValid());
	SPT_CHECK(queryPool.IsValid());

	vkCmdWriteTimestamp2(m_cmdBufferHandle, RHIToVulkan::GetStageFlags(stage), queryPool.GetHandle(), queryIdx);
}

void RHICommandBuffer::BeginQuery(const RHIQueryPool& queryPool, Uint32 queryIdx)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(IsValid());
	SPT_CHECK(queryPool.IsValid());

	vkCmdBeginQuery(m_cmdBufferHandle, queryPool.GetHandle(), queryIdx, 0);
}

void RHICommandBuffer::EndQuery(const RHIQueryPool& queryPool, Uint32 queryIdx)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(IsValid());
	SPT_CHECK(queryPool.IsValid());

	vkCmdEndQuery(m_cmdBufferHandle, queryPool.GetHandle(), queryIdx);
}

VkCommandBuffer RHICommandBuffer::GetHandle() const
{
	return m_cmdBufferHandle;
}

void RHICommandBuffer::BindPipelineImpl(VkPipelineBindPoint bindPoint, const RHIPipeline& pipeline)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(IsValid());
	SPT_CHECK(pipeline.IsValid());

	vkCmdBindPipeline(m_cmdBufferHandle, bindPoint, pipeline.GetHandle());
}

void RHICommandBuffer::BindDescriptorSetImpl(VkPipelineBindPoint bindPoint, const RHIPipeline& pipeline, const RHIDescriptorSet& ds, Uint32 dsIdx, const Uint32* dynamicOffsets, Uint32 dynamicOffsetsNum)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(IsValid());
	SPT_CHECK(ds.IsValid());
	SPT_CHECK(pipeline.IsValid());
	SPT_CHECK(dsIdx != idxNone<Uint32>);
	SPT_CHECK(dynamicOffsetsNum == 0 || dynamicOffsets != nullptr);

	const PipelineLayout& layout = pipeline.GetPipelineLayout();
	VkDescriptorSet dsHandle = ds.GetHandle();

	vkCmdBindDescriptorSets(m_cmdBufferHandle, bindPoint, layout.GetHandle(), dsIdx, 1, &dsHandle, dynamicOffsetsNum, dynamicOffsets);
}

void RHICommandBuffer::BuildASImpl(const RHIAccelerationStructure& as, VkAccelerationStructureBuildGeometryInfoKHR& buildInfo, const RHIBuffer& scratchBuffer, Uint64 scratchBufferOffset)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(IsValid());
	SPT_CHECK(as.IsValid());
	SPT_CHECK(scratchBuffer.IsValid());

	SPT_CHECK(scratchBufferOffset + as.GetBuildScratchSize() <= scratchBuffer.GetSize());

	buildInfo.dstAccelerationStructure	= as.GetHandle();
	buildInfo.scratchData.deviceAddress	= scratchBuffer.GetDeviceAddress() + scratchBufferOffset;

	const VkAccelerationStructureBuildRangeInfoKHR buildRangeInfo = as.CreateBuildRangeInfo();
	const VkAccelerationStructureBuildRangeInfoKHR* buildRanges = &buildRangeInfo;

	vkCmdBuildAccelerationStructuresKHR(m_cmdBufferHandle, 1, &buildInfo, &buildRanges);
}

} // spt::vulkan
