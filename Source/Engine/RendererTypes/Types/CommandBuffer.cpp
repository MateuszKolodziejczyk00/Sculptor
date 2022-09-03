#include "CommandBuffer.h"
#include "RendererUtils.h"

namespace spt::rdr
{

CommandBuffer::CommandBuffer(const RendererResourceName& name, const rhi::CommandBufferDefinition& definition)
{
	SPT_PROFILE_FUNCTION();

	GetRHI().InitializeRHI(definition);
	GetRHI().SetName(name.Get());
}

void CommandBuffer::StartRecording(const rhi::CommandBufferUsageDefinition& usageDefinition)
{
	GetRHI().StartRecording(usageDefinition);
}

void CommandBuffer::FinishRecording()
{
	GetRHI().StopRecording();
}

}
