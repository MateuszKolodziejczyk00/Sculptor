#include "CommandBuffer.h"
#include "CurrentFrameContext.h"
#include "RendererUtils.h"

namespace spt::renderer
{

CommandBuffer::CommandBuffer(const RendererResourceName& name, const rhi::CommandBufferDefinition& definition)
{
	SPT_PROFILE_FUNCTION();

	m_cmdBuffer.InitializeRHI(definition);
	m_cmdBuffer.SetName(name.Get());
}

CommandBuffer::~CommandBuffer()
{
	SPT_PROFILE_FUNCTION();

	CurrentFrameContext::GetCurrentFrameCleanupDelegate().AddLambda(
		[resource = m_cmdBuffer]() mutable
		{
			resource.ReleaseRHI();
		});
}

rhi::RHICommandBuffer& CommandBuffer::GetRHI()
{
	return m_cmdBuffer;
}

const rhi::RHICommandBuffer& CommandBuffer::GetRHI() const
{
	return m_cmdBuffer;
}

void CommandBuffer::StartRecording(const rhi::CommandBufferUsageDefinition& usageDefinition)
{
	m_cmdBuffer.StartRecording(usageDefinition);
}

void CommandBuffer::FinishRecording()
{
	m_cmdBuffer.StopRecording();
}

}
