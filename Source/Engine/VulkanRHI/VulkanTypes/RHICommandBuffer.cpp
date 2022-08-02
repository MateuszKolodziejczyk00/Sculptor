#include "RHICommandBuffer.h"
#include "VulkanRHI.h"
#include "CommandPool/RHICommandPoolsManager.h"
#include "LayoutsManager.h"

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

	m_cmdBufferHandle = VulkanRHI::GetCommandPoolsManager().AcquireCommandBuffer(bufferDefinition, m_acquireInfo);
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

}
