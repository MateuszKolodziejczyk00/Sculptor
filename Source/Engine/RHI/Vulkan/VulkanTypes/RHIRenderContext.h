#pragma once

#include "RHIMacros.h"
#include "SculptorCoreTypes.h"
#include "RHICore/RHIRenderContextTypes.h"
#include "RHICore/RHIDescriptorTypes.h"
#include "Vulkan/Debug/DebugUtils.h"
#include "RHIDescriptorSetLayout.h"


namespace spt::rhi
{
struct CommandBufferDefinition;
} // spt::rhi


namespace spt::vulkan
{

class DescriptorPoolSet;
class CommandPoolsLibrary;


struct RHI_API RHIRenderContextReleaseTicket
{
	void ExecuteReleaseRHI();

	RHIResourceReleaseTicket<CommandPoolsLibrary*> commandPoolsLibrary;

#if SPT_RHI_DEBUG
	lib::HashedString name;
#endif // SPT_RHI_DEBUG
};


class RHI_API RHIRenderContext
{
public:

	RHIRenderContext();
	~RHIRenderContext();

	RHIRenderContext(RHIRenderContext&& other);
	RHIRenderContext& operator=(RHIRenderContext&& rhs);

	void InitializeRHI(const rhi::ContextDefinition& definition);
	void ReleaseRHI();

	RHIRenderContextReleaseTicket DeferredReleaseRHI();

	Bool IsValid() const;

	rhi::ContextID GetID() const;

	void                     SetName(const lib::HashedString& name);
	const lib::HashedString& GetName() const;
	
	// Command Buffers ======================================================
	
	VkCommandBuffer AcquireCommandBuffer(const rhi::CommandBufferDefinition& cmdBufferDef);

private:

	rhi::ContextID m_id;

	lib::UniquePtr<CommandPoolsLibrary> m_commandPoolsLibrary;

	DebugName m_name;
};

} // spt::vulkan