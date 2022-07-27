#pragma once

#include "RendererTypesMacros.h"
#include "SculptorCoreTypes.h"
#include "RHICommandBufferImpl.h"


namespace spt::rhi
{
struct CommandBufferDefinition;
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

private:

	rhi::RHICommandBuffer			m_cmdBuffer;
};

}
