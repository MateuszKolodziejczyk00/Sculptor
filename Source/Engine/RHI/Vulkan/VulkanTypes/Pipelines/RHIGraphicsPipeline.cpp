#include "RHIGraphicsPipeline.h"
#include "Vulkan/VulkanRHI.h"

namespace spt::vulkan
{

RHIGraphicsPipeline::RHIGraphicsPipeline()
	: m_pipelineHandle(VK_NULL_HANDLE)
{ }

void RHIGraphicsPipeline::InitializeRHI(const rhi::GraphicsPipelineDefinition& pipelineDefinition, const rhi::PipelineLayoutDefinition& layoutDefinition)
{
	SPT_PROFILE_FUNCTION();

	SPT_CHECK(!IsValid());

	m_layout.InitializeRHI(layoutDefinition);

	m_debugName.Reset();
}

void RHIGraphicsPipeline::ReleaseRHI()
{
	SPT_PROFILE_FUNCTION();

	SPT_CHECK(IsValid());

	vkDestroyPipeline(VulkanRHI::GetDeviceHandle(), m_pipelineHandle, VulkanRHI::GetAllocationCallbacks());
	m_pipelineHandle = VK_NULL_HANDLE;

	m_layout.ReleaseRHI();
}

Bool RHIGraphicsPipeline::IsValid() const
{
	return GetHandle() != VK_NULL_HANDLE;
}

void RHIGraphicsPipeline::SetName(const lib::HashedString& name)
{
	SPT_CHECK(IsValid());

	m_debugName.Set(name, reinterpret_cast<Uint64>(m_pipelineHandle), VK_OBJECT_TYPE_PIPELINE);
}

const lib::HashedString& RHIGraphicsPipeline::GetName() const
{
	return m_debugName.Get();
}

VkPipeline RHIGraphicsPipeline::GetHandle() const
{
	return m_pipelineHandle;
}

} // spt::vulkan