#include "RHIPipeline.h"
#include "Vulkan/VulkanRHI.h"

namespace spt::vulkan
{

RHIPipeline::RHIPipeline()
	: m_pipelineHandle(VK_NULL_HANDLE)
{ }

void RHIPipeline::InitializeRHI(const rhi::PipelineShaderStagesDefinition& shaderStagesDef, const rhi::GraphicsPipelineDefinition& pipelineDefinition, const rhi::PipelineLayoutDefinition& layoutDefinition)
{
	SPT_PROFILE_FUNCTION();

	SPT_CHECK(!IsValid());

	m_layout.InitializeRHI(layoutDefinition);

	m_debugName.Reset();
}

void RHIPipeline::ReleaseRHI()
{
	SPT_PROFILE_FUNCTION();

	SPT_CHECK(IsValid());

	vkDestroyPipeline(VulkanRHI::GetDeviceHandle(), m_pipelineHandle, VulkanRHI::GetAllocationCallbacks());
	m_pipelineHandle = VK_NULL_HANDLE;

	m_layout.ReleaseRHI();
}

Bool RHIPipeline::IsValid() const
{
	return GetHandle() != VK_NULL_HANDLE;
}

void RHIPipeline::SetName(const lib::HashedString& name)
{
	SPT_CHECK(IsValid());

	m_debugName.Set(name, reinterpret_cast<Uint64>(m_pipelineHandle), VK_OBJECT_TYPE_PIPELINE);
}

const lib::HashedString& RHIPipeline::GetName() const
{
	return m_debugName.Get();
}

VkPipeline RHIPipeline::GetHandle() const
{
	return m_pipelineHandle;
}

} // spt::vulkan