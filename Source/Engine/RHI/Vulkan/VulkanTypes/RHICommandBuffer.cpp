#include "RHICommandBuffer.h"
#include "Vulkan/VulkanRHI.h"
#include "Vulkan/CommandPool/RHICommandPoolsManager.h"
#include "Vulkan/LayoutsManager.h"
#include "Vulkan/RHIToVulkanCommon.h"
#include "RHITexture.h"
#include "RHICore/Commands/RHIRenderingDefinition.h"

namespace spt::vulkan
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// Helpers =======================================================================================

namespace priv
{

VkCommandBufferUsageFlags GetVulkanCommandBufferUsageFlags(Flags32 rhiFlags)
{
	VkCommandBufferUsageFlags usage = 0;

	if ((rhiFlags & rhi::ECommandBufferBeginFlags::OneTimeSubmit) != 0)
	{
		usage |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	}
	if ((rhiFlags & rhi::ECommandBufferBeginFlags::ContinueRendering) != 0)
	{
		usage |= VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;
	}

	return usage;
}

}

//////////////////////////////////////////////////////////////////////////////////////////////////
// RHICommandBuffer ==============================================================================

RHICommandBuffer::RHICommandBuffer()
	: m_cmdBufferHandle(VK_NULL_HANDLE)
{ }

void RHICommandBuffer::InitializeRHI(const rhi::CommandBufferDefinition& bufferDefinition)
{
	SPT_PROFILE_FUNCTION();

	SPT_CHECK(!IsValid());

	m_cmdBufferHandle	= VulkanRHI::GetCommandPoolsManager().AcquireCommandBuffer(bufferDefinition, m_acquireInfo);
	m_queueType			= bufferDefinition.m_queueType;
	m_cmdBufferType		= bufferDefinition.m_cmdBufferType;
}

void RHICommandBuffer::ReleaseRHI()
{
	SPT_PROFILE_FUNCTION();

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
	beginInfo.flags = priv::GetVulkanCommandBufferUsageFlags(usageDefinition.m_beginFlags);

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
	SPT_PROFILE_FUNCTION();

	SPT_CHECK(IsValid());
	
	const auto CreateAttachmentInfo = [this](const rhi::RHIRenderTargetDefinition& renderTarget, Bool isColor)
	{
		LayoutsManager& layoutsManager = VulkanRHI::GetLayoutsManager();

		VkRenderingAttachmentInfo attachmentInfo{ VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };

		const RHITextureView& textureView = renderTarget.m_textureView;
		SPT_CHECK(textureView.IsValid() && textureView.GetTexture());
		const RHITexture& texture = *textureView.GetTexture();

		attachmentInfo.imageView = textureView.GetHandle();
		attachmentInfo.imageLayout = layoutsManager.GetSubresourcesSharedLayout(m_cmdBufferHandle,
																				texture.GetHandle(),
																				textureView.GetSubresourceRange());

		if (renderTarget.m_resolveTextureView.IsValid())
		{
			const RHITextureView& resolveTextureView = renderTarget.m_resolveTextureView;
			SPT_CHECK(!!resolveTextureView.GetTexture());
			const RHITexture& resolveTexture = *resolveTextureView.GetTexture();

			attachmentInfo.resolveMode = RHIToVulkan::GetResolveMode(renderTarget.m_resolveMode);
			attachmentInfo.resolveImageView = resolveTextureView.GetHandle();
			attachmentInfo.resolveImageLayout = layoutsManager.GetSubresourcesSharedLayout(	m_cmdBufferHandle,
																							resolveTexture.GetHandle(),
																							resolveTextureView.GetSubresourceRange());
		}

		attachmentInfo.loadOp = RHIToVulkan::GetLoadOp(renderTarget.m_loadOperation);
		attachmentInfo.storeOp = RHIToVulkan::GetStoreOp(renderTarget.m_storeOperation);
		if (isColor)
		{
			memcpy(&attachmentInfo.clearValue, &renderTarget.m_clearColor, sizeof(VkClearValue));
		}
		else
		{
			attachmentInfo.clearValue.depthStencil.depth	= renderTarget.m_clearColor.m_depthStencil.m_depth;
			attachmentInfo.clearValue.depthStencil.stencil	= renderTarget.m_clearColor.m_depthStencil.m_stencil;
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
	colorAttachments.reserve(renderingDefinition.m_colorRTs.size());

	std::transform(	renderingDefinition.m_colorRTs.cbegin(), renderingDefinition.m_colorRTs.end(),
					std::back_inserter(colorAttachments),
					CreateColorAttachmentInfo);

	const Bool hasDepthAttachment = renderingDefinition.m_depthRT.m_textureView.IsValid();
	VkRenderingAttachmentInfo depthAttachmentInfo{};
	if (hasDepthAttachment)
	{
		depthAttachmentInfo = CreateDepthStencilAttachmentInfo(renderingDefinition.m_depthRT);
	}

	const Bool hasStencilAttachment = renderingDefinition.m_stencilRT.m_textureView.IsValid();
	VkRenderingAttachmentInfo stencilAttachmentInfo{};
	if (hasStencilAttachment)
	{
		stencilAttachmentInfo = CreateDepthStencilAttachmentInfo(renderingDefinition.m_stencilRT);
	}

	const math::Vector2i areaOffset = renderingDefinition.m_renderAreaOffset;
	const math::Vector2u areaExtent = renderingDefinition.m_renderAreaExtent;

	VkRenderingInfo renderingInfo{ VK_STRUCTURE_TYPE_RENDERING_INFO };
    renderingInfo.flags					= RHIToVulkan::GetRenderingFlags(renderingDefinition.m_renderingFlags);
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
	SPT_PROFILE_FUNCTION();

	SPT_CHECK(IsValid());

	vkCmdEndRendering(m_cmdBufferHandle);
}

}
