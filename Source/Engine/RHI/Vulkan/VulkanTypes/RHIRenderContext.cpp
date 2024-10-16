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

RHIRenderContext::RHIRenderContext()
	: m_id(idxNone<rhi::ContextID>)
{ }

// default destructor and move operations in .cpp file, to limit necessary includes in .h file
RHIRenderContext::~RHIRenderContext() = default;
RHIRenderContext::RHIRenderContext(RHIRenderContext&& other) = default;
RHIRenderContext& RHIRenderContext::operator=(RHIRenderContext&& rhs) = default;

void RHIRenderContext::InitializeRHI(const rhi::ContextDefinition& definition)
{
	m_id = priv::GenerateID();
}

void RHIRenderContext::ReleaseRHI()
{
	m_id = idxNone<rhi::ContextID>;

	if (m_commandPoolsLibrary)
	{
		VulkanRHI::GetCommandPoolsManager().ReleaseCommandPoolsLibrary(std::move(m_commandPoolsLibrary));
	}

	m_name.Reset(0, VK_OBJECT_TYPE_UNKNOWN);
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
