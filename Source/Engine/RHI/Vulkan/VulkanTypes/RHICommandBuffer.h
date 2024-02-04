#pragma once

#include "RHIMacros.h"
#include "Vulkan/VulkanCore.h"
#include "Vulkan/Debug/DebugUtils.h"
#include "RHICore/RHICommandBufferTypes.h"

#include "RHICore/Commands/RHIRenderingDefinition.h"
#include "RHICore/Commands/RHICopyDefinition.h"
#include "RHICore/RHISamplerTypes.h"
#include "RHICore/RHITextureTypes.h"

namespace spt::vulkan
{

class RHIRenderContext;
class RHIPipeline;
class RHIDescriptorSet;
class RHITexture;
class RHIBuffer;
class RHIAccelerationStructure;
class RHIBottomLevelAS;
class RHITopLevelAS;
class RHIShaderBindingTable;
class RHIQueryPool;


class RHI_API RHICommandBuffer
{
public:

	RHICommandBuffer();

	void							InitializeRHI(RHIRenderContext& renderContext, const rhi::CommandBufferDefinition& bufferDefinition);
	void							ReleaseRHI();

	Bool							IsValid() const;

	rhi::EDeviceCommandQueueType	GetQueueType() const;

	void							StartRecording(const rhi::CommandBufferUsageDefinition& usageDefinition);
	void							StopRecording();

	void							SetName(const lib::HashedString& name);
	const lib::HashedString&		GetName() const;

	// Gfx rendering ========================================

	void	BeginRendering(const rhi::RenderingDefinition& renderingDefinition);
	void	EndRendering();

	void	SetViewport(const math::AlignedBox2f& renderingViewport, Real32 minDepth, Real32 maxDepth);
	void	SetScissor(const math::AlignedBox2u& renderingScissor);

	void	DrawIndirectCount(const RHIBuffer& drawsBuffer, Uint64 drawsOffset, Uint32 drawsStride, const RHIBuffer& countBuffer, Uint64 countOffset, Uint32 maxDrawsCount);
	void	DrawIndirect(const RHIBuffer& drawsBuffer, Uint64 drawsOffset, Uint32 drawsStride, Uint32 drawsCount);

	void	DrawInstances(Uint32 verticesNum, Uint32 instancesNum, Uint32 firstVertex, Uint32 firstInstance);

	void	BindGfxPipeline(const RHIPipeline& pipeline);

	void	BindGfxDescriptorSet(const RHIPipeline& pipeline, const RHIDescriptorSet& ds, Uint32 dsIdx, const Uint32* dynamicOffsets, Uint32 dynamicOffsetsNum);

	// Compute rendering ====================================

	void	BindComputePipeline(const RHIPipeline& pipeline);

	void	BindComputeDescriptorSet(const RHIPipeline& pipeline, const RHIDescriptorSet& ds, Uint32 dsIdx, const Uint32* dynamicOffsets, Uint32 dynamicOffsetsNum);

	void	Dispatch(const math::Vector3u& groupCount);
	void	DispatchIndirect(const RHIBuffer& indirectArgsBuffer, Uint64 indirectArgsOffset);

	// Acceleration Structures ==============================

	void	BuildBLAS(const RHIBottomLevelAS& blas, const RHIBuffer& scratchBuffer, Uint64 scratchBufferOffset);
	void	BuildTLAS(const RHITopLevelAS& tlas, const RHIBuffer& scratchBuffer, Uint64 scratchBufferOffset, const RHIBuffer& instancesBuildDataBuffer);
	
	// Ray Tracing ==========================================

	void	BindRayTracingPipeline(const RHIPipeline& pipeline);

	void 	BindRayTracingDescriptorSet(const RHIPipeline& pipeline, const RHIDescriptorSet& ds, Uint32 dsIdx, const Uint32* dynamicOffsets, Uint32 dynamicOffsetsNum);

	void 	TraceRays(const RHIShaderBindingTable& sbt, const math::Vector3u& traceCount);

	// Transfer =============================================

	void	BlitTexture(const RHITexture& source, Uint32 sourceMipLevel, Uint32 sourceArrayLayer, const RHITexture& dest, Uint32 destMipLevel, Uint32 destArrayLayer, rhi::ETextureAspect aspect, rhi::ESamplerFilterType filterMode);

	void	ClearTexture(const RHITexture& texture, const rhi::ClearColor& clearColor, const rhi::TextureSubresourceRange& subresourceRange);

	void	CopyTexture(const RHITexture& source, const rhi::TextureCopyRange& sourceRange, const RHITexture& target, const rhi::TextureCopyRange& targetRange, const math::Vector3u& extent);
	void	CopyBuffer(const RHIBuffer& sourceBuffer, Uint64 sourceOffset, const RHIBuffer& destBuffer, Uint64 destOffset, Uint64 size);
	void	FillBuffer(const RHIBuffer& buffer, Uint64 offset, Uint64 range, Uint32 data);
	
	void	CopyBufferToTexture(const RHIBuffer& buffer, Uint64 bufferOffset, const RHITexture& texture, rhi::ETextureAspect aspect, math::Vector3u copyExtent, math::Vector3u copyOffset = math::Vector3u::Zero(),  Uint32 mipLevel = 0, Uint32 arrayLayer = 0);

	// Utils ============================================

	void ExecuteCommands(const RHICommandBuffer& secondaryCommandBuffer);

	// Debug ============================================

	void	BeginDebugRegion(const lib::HashedString& name, const lib::Color& color);
	void	EndDebugRegion();

#if SPT_ENABLE_GPU_CRASH_DUMPS
	void	SetDebugCheckpoint(const void* markerPtr);
#endif // SPT_ENABLE_GPU_CRASH_DUMPS

	void	ResetQueryPool(const RHIQueryPool& queryPool, Uint32 firstQueryIdx, Uint32 queryCount);
	
	void	WriteTimestamp(const RHIQueryPool& queryPool, Uint32 queryIdx, rhi::EPipelineStage stage);

	void	BeginQuery(const RHIQueryPool& queryPool, Uint32 queryIdx);
	void	EndQuery(const RHIQueryPool& queryPool, Uint32 queryIdx);

	// Vulkan specific ======================================

	VkCommandBuffer	 GetHandle() const;

	operator VkCommandBuffer() const
	{
		return GetHandle();
	}

private:

	void BindPipelineImpl(VkPipelineBindPoint bindPoint, const RHIPipeline& pipeline);
	void BindDescriptorSetImpl(VkPipelineBindPoint bindPoint, const RHIPipeline& pipeline, const RHIDescriptorSet& ds, Uint32 dsIdx, const Uint32* dynamicOffsets, Uint32 dynamicOffsetsNum);

	void BuildASImpl(const RHIAccelerationStructure& as, VkAccelerationStructureBuildGeometryInfoKHR& buildInfo, const RHIBuffer& scratchBuffer, Uint64 scratchBufferOffset);

	VkCommandBuffer					m_cmdBufferHandle;

	rhi::EDeviceCommandQueueType	m_queueType;
	rhi::ECommandBufferType			m_cmdBufferType;

	DebugName						m_name;
};

} // spt::vulkan