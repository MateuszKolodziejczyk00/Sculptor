#include "RHICommandBuffer.h"
#include "VulkanRHI.h"
#include "CommandPool/RHICommandPoolsManager.h"

namespace spt::vulkan
{

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

void RHICommandBuffer::SetName(const lib::HashedString& name)
{
	m_name.Set(name, reinterpret_cast<Uint64>(m_cmdBufferHandle), VK_OBJECT_TYPE_COMMAND_BUFFER);
}

const lib::HashedString& RHICommandBuffer::GetName() const
{
	return m_name.Get();
}

}
