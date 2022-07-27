#pragma once

#include "RendererTypesMacros.h"
#include "SculptorCoreTypes.h"
#include "RHICommandBufferImpl.h"


namespace spt::rhi
{
struct CommandBufferDefinition;
struct CommandBufferUsageDefinition;
}


namespace spt::renderer
{

struct RendererResourceName;


class RENDERER_TYPES_API CommandBuffer
{
public:

	CommandBuffer(const RendererResourceName& name, const rhi::CommandBufferDefinition& definition);
	~CommandBuffer();

	rhi::RHICommandBuffer&			GetRHI();
	const rhi::RHICommandBuffer&	GetRHI() const;

	void							StartRecording(const rhi::CommandBufferUsageDefinition& usageDefinition);
	void							FinishRecording();

private:

	rhi::RHICommandBuffer			m_cmdBuffer;
};

}
