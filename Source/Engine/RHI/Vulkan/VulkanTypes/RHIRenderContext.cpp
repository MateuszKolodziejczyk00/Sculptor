#include "RHIRenderContext.h"
#include "Vulkan/DescriptorSets/RHIDescriptorSetManager.h"
#include "Vulkan/VulkanRHIUtils.h"
#include "Vulkan/VulkanRHI.h"
#include "Vulkan/CommandPool/RHICommandPoolsManager.h"

namespace spt::vulkan
{

namespace priv
{

static rhi::ContextID GenerateID()
{
	static std::atomic<rhi::ContextID> context = 1;
	return ++context;
}

} // priv

//////////////////////////////////////////////////////////////////////////////////////////////////
// RHIRenderContextReleaseTicket =================================================================

void RHIRenderContextReleaseTicket::ExecuteReleaseRHI()
{
	if (commandPoolsLibrary.IsValid())
	{
		VulkanRHI::GetCommandPoolsManager().ReleaseCommandPoolsLibrary(std::unique_ptr<CommandPoolsLibrary>(commandPoolsLibrary.GetValue()));
		commandPoolsLibrary.Reset();
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// RHIRenderContext ==============================================================================

RHIRenderContext::RHIRenderContext()
	: m_id(idxNone<rhi::ContextID>)
{ }

// default destructor and move operations in .cpp file, to limit necessary includes in .h file
RHIRenderContext::~RHIRenderContext() = default;
RHIRenderContext::RHIRenderContext(RHIRenderContext&& other) = default;
RHIRenderContext& RHIRenderContext::operator=(RHIRenderContext&& rhs) = default;

void RHIRenderContext::InitializeRHI(const rhi::ContextDefinition& definition)
{
	SPT_CHECK(!IsValid());

	m_id = priv::GenerateID();

	SPT_CHECK(IsValid());
}

void RHIRenderContext::ReleaseRHI()
{
	RHIRenderContextReleaseTicket releaseTicket = DeferredReleaseRHI();
	releaseTicket.ExecuteReleaseRHI();
}

RHIRenderContextReleaseTicket RHIRenderContext::DeferredReleaseRHI()
{
	SPT_CHECK(IsValid());

	RHIRenderContextReleaseTicket releaseTicket;
	releaseTicket.commandPoolsLibrary = m_commandPoolsLibrary.release();

#if SPT_RHI_DEBUG
	releaseTicket.name = GetName();
#endif // SPT_RHI_DEBUG

	m_id = idxNone<rhi::ContextID>;
	m_name.Reset(0, VK_OBJECT_TYPE_UNKNOWN);

	SPT_CHECK(!IsValid());

	return releaseTicket;
}

Bool RHIRenderContext::IsValid() const
{
	return m_id != idxNone<rhi::ContextID>;
}

rhi::ContextID RHIRenderContext::GetID() const
{
	return m_id;
}

void RHIRenderContext::SetName(const lib::HashedString& name)
{
	m_name.SetWithoutObject(name);
}

const lib::HashedString& RHIRenderContext::GetName() const
{
	return m_name.Get();
}

VkCommandBuffer RHIRenderContext::AcquireCommandBuffer(const rhi::CommandBufferDefinition& cmdBufferDef)
{
	if (!m_commandPoolsLibrary)
	{
		m_commandPoolsLibrary = VulkanRHI::GetCommandPoolsManager().AcquireCommandPoolsLibrary();
	}

	return m_commandPoolsLibrary->AcquireCommandBuffer(cmdBufferDef);
}

} // spt::vulkan
