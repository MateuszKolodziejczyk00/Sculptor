#include "PipelineLayout.h"
#include "Vulkan/VulkanRHI.h"
#include "PipelineLayoutsManager.h"

namespace spt::vulkan
{

PipelineLayout::PipelineLayout()
	: m_layoutHnadle(VK_NULL_HANDLE)
{ }

void PipelineLayout::InitializeRHI(const rhi::PipelineLayoutDefinition& definition)
{
	SPT_PROFILE_FUNCTION();

	m_layoutHnadle = VulkanRHI::GetPipelineLayoutsManager().GetOrCreatePipelineLayout(definition);
}

void PipelineLayout::ReleaseRHI()
{
	SPT_CHECK(IsValid());

	// We don't destroy layout, as its managed by PipelineLayoutsManager (and destroyed there)
	m_layoutHnadle = VK_NULL_HANDLE;
}

Bool PipelineLayout::IsValid() const
{
	return m_layoutHnadle != VK_NULL_HANDLE;
}

VkPipelineLayout PipelineLayout::GetHandle() const
{
	return m_layoutHnadle;

}

} // spt::vulkan
