#pragma once

#include "RHIMacros.h"
#include "Vulkan/VulkanCore.h"
#include "Vulkan/CommandPool/RHICommandPoolsTypes.h"
#include "Vulkan/Debug/DebugUtils.h"
#include "RHICore/RHICommandBufferTypes.h"

#include "RHICore/Commands/RHIRenderingDefinition.h"
#include "RHICore/Commands/RHICopyDefinition.h"

namespace spt::vulkan
{

class RHIPipeline;
class RHIDescriptorSet;
class RHITexture;


class RHI_API RHICommandBuffer
{
public:

	RHICommandBuffer();

	void							InitializeRHI(const rhi::CommandBufferDefinition& bufferDefinition);
	void							ReleaseRHI();

	Bool							IsValid() const;

	VkCommandBuffer					GetHandle() const;

	rhi::ECommandBufferQueueType	GetQueueType() const;

	void							StartRecording(const rhi::CommandBufferUsageDefinition& usageDefinition);
	void							StopRecording();

	void							SetName(const lib::HashedString& name);
	const lib::HashedString&		GetName() const;

	// Gfx rendering ========================================

	void	BeginRendering(const rhi::RenderingDefinition& renderingDefinition);
	void	EndRendering();

	void	BindGfxPipeline(const RHIPipeline& pipeline);

	void	BindGfxDescriptorSet(const RHIPipeline& pipeline, const RHIDescriptorSet& ds, Uint32 dsIdx, const lib::DynamicArray<Uint32>& dynamicOffsets);

	// Compute rendering ====================================

	void	BindComputePipeline(const RHIPipeline& pipeline);

	void	BindComputeDescriptorSet(const RHIPipeline& pipeline, const RHIDescriptorSet& ds, Uint32 dsIdx, const lib::DynamicArray<Uint32>& dynamicOffsets);

	void	Dispatch(const math::Vector3u& groupCount);

	// Transfer =============================================

	void	CopyTexture(const RHITexture& source, const rhi::TextureCopyRange& sourceRange, const RHITexture& target, const rhi::TextureCopyRange& targetRange, const math::Vector3u& extent);

private:

	void BindPipelineImpl(VkPipelineBindPoint bindPoint, const RHIPipeline& pipeline);
	void BindDescriptorSetImpl(VkPipelineBindPoint bindPoint, const RHIPipeline& pipeline, const RHIDescriptorSet& ds, Uint32 dsIdx, const lib::DynamicArray<Uint32>& dynamicOffsets);

	VkCommandBuffer					m_cmdBufferHandle;

	rhi::ECommandBufferQueueType	m_queueType;
	rhi::ECommandBufferType			m_cmdBufferType;

	CommandBufferAcquireInfo		m_acquireInfo;

	DebugName						m_name;
};

}