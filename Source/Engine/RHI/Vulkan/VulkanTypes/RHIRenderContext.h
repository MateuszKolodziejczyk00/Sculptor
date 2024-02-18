#pragma once

#include "RHIMacros.h"
#include "SculptorCoreTypes.h"
#include "RHICore/RHIRenderContextTypes.h"
#include "RHICore/RHIDescriptorTypes.h"
#include "Vulkan/Debug/DebugUtils.h"
#include "RHIDescriptorSet.h"
#include "RHIDescriptorSetLayout.h"


namespace spt::rhi
{
struct CommandBufferDefinition;
} // spt::rhi


namespace spt::vulkan
{

class DescriptorPoolSet;
class CommandPoolsLibrary;


class RHI_API RHIRenderContext
{
public:

	RHIRenderContext();
	~RHIRenderContext();

	RHIRenderContext(RHIRenderContext&& other);
	RHIRenderContext& operator=(RHIRenderContext&& rhs);

	void						InitializeRHI(const rhi::ContextDefinition& definition);
	void						ReleaseRHI();

	Bool						IsValid() const;

	rhi::ContextID				GetID() const;

	void						SetName(const lib::HashedString& name);
	const lib::HashedString&	GetName() const;
	
	// Descriptor sets ======================================================

	/** Allocates dynamic descriptor sets bound to this context. These descriptors are automatically destroyed with this context */
	SPT_NODISCARD lib::DynamicArray<RHIDescriptorSet> AllocateDescriptorSets(const lib::Span<const RHIDescriptorSetLayout> layouts);

	// Command Buffers ======================================================
	
	VkCommandBuffer AcquireCommandBuffer(const rhi::CommandBufferDefinition& cmdBufferDef);

private:

	rhi::ContextID m_id;

	lib::UniquePtr<DescriptorPoolSet> m_dynamicDescriptorsPool;

	lib::UniquePtr<CommandPoolsLibrary> m_commandPoolsLibrary;

	DebugName m_name;
};

} // spt::vulkan