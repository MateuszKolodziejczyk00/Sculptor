#pragma once

#include "RHIMacros.h"
#include "RHICore/RHICommandBufferTypes.h"
#include "Vulkan/Vulkan123.h"
#include "Vulkan/CommandPool/RHICommandPoolsTypes.h"
#include "Vulkan/Debug/DebugUtils.h"


namespace spt::vulkan
{

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

private:

	VkCommandBuffer					m_cmdBufferHandle;

	rhi::ECommandBufferQueueType	m_queueType;
	rhi::ECommandBufferType			m_cmdBufferType;

	CommandBufferAcquireInfo		m_acquireInfo;

	DebugName						m_name;
};

}