#pragma once

#include "RendererCoreMacros.h"
#include "RHIBridge/RHICommandBufferImpl.h"
#include "RendererResource.h"
#include "SculptorCoreTypes.h"


namespace spt::rhi
{
struct CommandBufferDefinition;
struct CommandBufferUsageDefinition;
}


namespace spt::rdr
{

struct RendererResourceName;


class RENDERER_CORE_API CommandBuffer : public RendererResource<rhi::RHICommandBuffer>
{
protected:

	using ResourceType = RendererResource<rhi::RHICommandBuffer>;

public:

	CommandBuffer(const RendererResourceName& name, const rhi::CommandBufferDefinition& definition);

	void StartRecording(const rhi::CommandBufferUsageDefinition& usageDefinition);
	void FinishRecording();
};

}
