#include "CommandBuffer.h"
#include "RendererUtils.h"
#include "RenderContext.h"

namespace spt::rdr
{

CommandBuffer::CommandBuffer(const RendererResourceName& name, const lib::SharedRef<RenderContext>& renderContext, const rhi::CommandBufferDefinition& definition)
{
	SPT_PROFILER_FUNCTION();

	GetRHI().InitializeRHI(renderContext->GetRHI(), definition);
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

} // spt::rdr
