#include "RHIPipeline.h"
#include "Vulkan/VulkanRHI.h"
#include "Vulkan/Pipeline/PipelinesManager.h"

namespace spt::vulkan
{

RHIPipeline::RHIPipeline()
	: m_pipelineID(idxNone<PipelineID>)
	, m_cachedPipelineHandle(VK_NULL_HANDLE)
	, m_pipelineType(rhi::EPipelineType::None)
{ }

void RHIPipeline::InitializeRHI(const rhi::PipelineShaderStagesDefinition& shaderStagesDef, const rhi::GraphicsPipelineDefinition& pipelineDefinition, const rhi::PipelineLayoutDefinition& layoutDefinition)
{
	SPT_PROFILE_FUNCTION();

	SPT_CHECK(!IsValid());

	m_layout.InitializeRHI(layoutDefinition);

	InitializeGraphicsPipeline(shaderStagesDef, pipelineDefinition, m_layout);
}

void RHIPipeline::InitializeRHI(const rhi::RHIShaderModule& computeShaderModule, const rhi::PipelineLayoutDefinition& layoutDefinition)
{
	SPT_PROFILE_FUNCTION();

	SPT_CHECK(!IsValid());

	m_layout.InitializeRHI(layoutDefinition);

	InitializeComputePipeline(computeShaderModule, m_layout);
}

void RHIPipeline::ReleaseRHI()
{
	SPT_PROFILE_FUNCTION();

	SPT_CHECK(IsValid());

	VulkanRHI::GetPipelinesManager().ReleasePipeline(m_pipelineID);
	m_pipelineID = idxNone<PipelineID>;
	m_cachedPipelineHandle = VK_NULL_HANDLE;
	m_pipelineType = rhi::EPipelineType::None;

	m_layout.ReleaseRHI();

	m_debugName.Reset();
}

Bool RHIPipeline::IsValid() const
{
	return m_pipelineID != idxNone<PipelineID>;
}

rhi::EPipelineType RHIPipeline::GetPipelineType() const
{
	return m_pipelineType;
}

void RHIPipeline::SetName(const lib::HashedString& name)
{
	SPT_CHECK(IsValid());

	// if we don't have cached handle, set name without object
	// in this case name will be set to pipeline after handle will be cached
	if (m_cachedPipelineHandle)
	{
		m_debugName.Set(name, reinterpret_cast<Uint64>(m_cachedPipelineHandle), VK_OBJECT_TYPE_PIPELINE);
	}
	else
	{
		m_debugName.SetWithoutObject(name);
	}
}

const lib::HashedString& RHIPipeline::GetName() const
{
	return m_debugName.Get();
}

VkPipeline RHIPipeline::GetHandle() const
{
	if (m_cachedPipelineHandle == VK_NULL_HANDLE)
	{
		CachePipelineHandle(VulkanRHI::GetPipelinesManager().GetPipelineHandle(m_pipelineID));
	}

	SPT_CHECK(!!m_cachedPipelineHandle);

	return m_cachedPipelineHandle;
}

void RHIPipeline::InitializeGraphicsPipeline(const rhi::PipelineShaderStagesDefinition& shaderStagesDef, const rhi::GraphicsPipelineDefinition& pipelineDefinition, const PipelineLayout& layout)
{
	SPT_PROFILE_FUNCTION();

	SPT_CHECK(layout.IsValid());

	const GraphicsPipelineBuildDefinition pipelineBuildDefinition(shaderStagesDef, pipelineDefinition, layout);
	m_pipelineID = VulkanRHI::GetPipelinesManager().BuildGraphicsPipelineDeferred(pipelineBuildDefinition);

	m_pipelineType = rhi::EPipelineType::Graphics;
}

void RHIPipeline::InitializeComputePipeline(const rhi::RHIShaderModule& computeShaderModule, const PipelineLayout& layout)
{
	SPT_PROFILE_FUNCTION();

	SPT_CHECK(layout.IsValid());

	const ComputePipelineBuildDefinition pipelineBuildDefinition(computeShaderModule, layout);
	m_pipelineID = VulkanRHI::GetPipelinesManager().BuildComputePipelineDeferred(pipelineBuildDefinition);

	m_pipelineType = rhi::EPipelineType::Compute;
}

void RHIPipeline::CachePipelineHandle(VkPipeline pipelineHandle) const
{
	SPT_CHECK(pipelineHandle != VK_NULL_HANDLE);

	m_cachedPipelineHandle = pipelineHandle;

#if VULKAN_VALIDATION
	if (m_debugName.HasName())
	{
		m_debugName.SetToObject(reinterpret_cast<Uint64>(m_cachedPipelineHandle), VK_OBJECT_TYPE_PIPELINE);
	}
#endif // VULKAN_VALIDATION
}

} // spt::vulkan