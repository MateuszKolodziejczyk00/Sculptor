#pragma once

#include "RHIMacros.h"
#include "Vulkan/VulkanCore.h"
#include "Vulkan/CommandPool/RHICommandPoolsTypes.h"
#include "Vulkan/Debug/DebugUtils.h"

#include "RHICore/RHICommandBufferTypes.h"

namespace spt::rhi
{
struct RenderingDefinition;
}


namespace spt::vulkan
{

class RHIPipeline;
class RHIDescriptorSet;


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

	void	BindGfxDescriptorSet(const RHIPipeline& pipeline, const RHIDescriptorSet& ds, Uint32 dsIdx);

	// Compute rendering ====================================

	void	BindComputePipeline(const RHIPipeline& pipeline);

private:

	void BindPipelineImpl(VkPipelineBindPoint bindPoint, const RHIPipeline& pipeline);
	void BindDescriptorSetImpl(VkPipelineBindPoint bindPoint, const RHIPipeline& pipeline, const RHIDescriptorSet& ds, Uint32 dsIdx);

	VkCommandBuffer					m_cmdBufferHandle;

	rhi::ECommandBufferQueueType	m_queueType;
	rhi::ECommandBufferType			m_cmdBufferType;

	CommandBufferAcquireInfo		m_acquireInfo;

	DebugName						m_name;
};

}