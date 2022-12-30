#pragma once

#include "RHIMacros.h"
#include "Vulkan/VulkanCore.h"
#include "Vulkan/Debug/DebugUtils.h"
#include "RHICore/RHICommandBufferTypes.h"

#include "RHICore/Commands/RHIRenderingDefinition.h"
#include "RHICore/Commands/RHICopyDefinition.h"

namespace spt::vulkan
{

class RHIRenderContext;
class RHIPipeline;
class RHIDescriptorSet;
class RHITexture;
class RHIBuffer;


class RHI_API RHICommandBuffer
{
public:

	RHICommandBuffer();

	void							InitializeRHI(RHIRenderContext& renderContext, const rhi::CommandBufferDefinition& bufferDefinition);
	void							ReleaseRHI();

	Bool							IsValid() const;

	rhi::ECommandBufferQueueType	GetQueueType() const;

	void							StartRecording(const rhi::CommandBufferUsageDefinition& usageDefinition);
	void							StopRecording();

	void							SetName(const lib::HashedString& name);
	const lib::HashedString&		GetName() const;

	// Gfx rendering ========================================

	void	BeginRendering(const rhi::RenderingDefinition& renderingDefinition);
	void	EndRendering();

	void	BindGfxPipeline(const RHIPipeline& pipeline);

	void	BindGfxDescriptorSet(const RHIPipeline& pipeline, const RHIDescriptorSet& ds, Uint32 dsIdx, const Uint32* dynamicOffsets, Uint32 dynamicOffsetsNum);

	// Compute rendering ====================================

	void	BindComputePipeline(const RHIPipeline& pipeline);

	void	BindComputeDescriptorSet(const RHIPipeline& pipeline, const RHIDescriptorSet& ds, Uint32 dsIdx, const Uint32* dynamicOffsets, Uint32 dynamicOffsetsNum);

	void	Dispatch(const math::Vector3u& groupCount);

	// Transfer =============================================

	void	CopyTexture(const RHITexture& source, const rhi::TextureCopyRange& sourceRange, const RHITexture& target, const rhi::TextureCopyRange& targetRange, const math::Vector3u& extent);
	void	CopyBuffer(const RHIBuffer& sourceBuffer, Uint64 sourceOffset, const RHIBuffer& destBuffer, Uint64 destOffset, Uint64 size);

	// Debug ============================================

	void	BeginDebugRegion(const lib::HashedString& name, const lib::Color& color);
	void	EndDebugRegion();

#if WITH_GPU_CRASH_DUMPS
	void	SetDebugCheckpoint(const void* markerPtr);
#endif // WITH_GPU_CRASH_DUMPS

	// Vulkan specific ======================================

	VkCommandBuffer					GetHandle() const;

	operator VkCommandBuffer() const
	{
		return GetHandle();
	}

private:

	void BindPipelineImpl(VkPipelineBindPoint bindPoint, const RHIPipeline& pipeline);
	void BindDescriptorSetImpl(VkPipelineBindPoint bindPoint, const RHIPipeline& pipeline, const RHIDescriptorSet& ds, Uint32 dsIdx, const Uint32* dynamicOffsets, Uint32 dynamicOffsetsNum);

	VkCommandBuffer					m_cmdBufferHandle;

	rhi::ECommandBufferQueueType	m_queueType;
	rhi::ECommandBufferType			m_cmdBufferType;

	DebugName						m_name;
};

} // spt::vulkan