#include "RHIPipeline.h"
#include "Vulkan/VulkanRHI.h"
#include "Vulkan/Pipeline/PipelinesManager.h"
#include "Vulkan/Pipeline/PipelineLayoutsManager.h"
#include "Vulkan/VulkanRHIUtils.h"

namespace spt::vulkan
{

RHIPipeline::RHIPipeline()
	: m_pipelineID(idxNone<PipelineID>)
	, m_cachedPipelineHandle(VK_NULL_HANDLE)
	, m_pipelineType(rhi::EPipelineType::None)
{ }

void RHIPipeline::InitializeRHI(const rhi::PipelineShaderStagesDefinition& shaderStagesDef, const rhi::GraphicsPipelineDefinition& pipelineDefinition, const rhi::PipelineLayoutDefinition& layoutDefinition)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(!IsValid());

	InitializePipelineLayout(layoutDefinition);

	SPT_CHECK(!!m_layout);

	InitializeGraphicsPipeline(shaderStagesDef, pipelineDefinition, *m_layout);
}

void RHIPipeline::InitializeRHI(const rhi::RHIShaderModule& computeShaderModule, const rhi::PipelineLayoutDefinition& layoutDefinition)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(!IsValid());

	InitializePipelineLayout(layoutDefinition);

	SPT_CHECK(!!m_layout);

	InitializeComputePipeline(computeShaderModule, *m_layout);
}

void RHIPipeline::ReleaseRHI()
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(IsValid());

	VulkanRHI::GetPipelinesManager().ReleasePipeline(m_pipelineID);
	m_pipelineID = idxNone<PipelineID>;
	m_cachedPipelineHandle = VK_NULL_HANDLE;
	m_pipelineType = rhi::EPipelineType::None;

	m_layout.reset();

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

rhi::DescriptorSetLayoutID RHIPipeline::GetDescriptorSetLayoutID(Uint32 bindingIdx) const
{
	SPT_CHECK(!!m_layout);

	return VulkanToRHI::GetDSLayoutID(m_layout->GetDescriptorSetLayout(bindingIdx));
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
	return IsValid() ? GetCachedPipelineHandle() : VK_NULL_HANDLE;
}

const PipelineLayout& RHIPipeline::GetPipelineLayout() const
{
	SPT_CHECK(IsValid());
	SPT_CHECK(!!m_layout);
	return *m_layout;
}

void RHIPipeline::InitializePipelineLayout(const rhi::PipelineLayoutDefinition& layoutDefinition)
{
	SPT_CHECK(!m_layout);

	m_layout = VulkanRHI::GetPipelineLayoutsManager().GetOrCreatePipelineLayout(layoutDefinition);
}

void RHIPipeline::InitializeGraphicsPipeline(const rhi::PipelineShaderStagesDefinition& shaderStagesDef, const rhi::GraphicsPipelineDefinition& pipelineDefinition, const PipelineLayout& layout)
{
	SPT_PROFILER_FUNCTION();

	const GraphicsPipelineBuildDefinition pipelineBuildDefinition(shaderStagesDef, pipelineDefinition, layout);
	m_pipelineID = VulkanRHI::GetPipelinesManager().BuildGraphicsPipelineDeferred(pipelineBuildDefinition);

	m_pipelineType = rhi::EPipelineType::Graphics;
}

void RHIPipeline::InitializeComputePipeline(const rhi::RHIShaderModule& computeShaderModule, const PipelineLayout& layout)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(computeShaderModule.IsValid());

	const ComputePipelineBuildDefinition pipelineBuildDefinition(computeShaderModule, layout);
	m_pipelineID = VulkanRHI::GetPipelinesManager().BuildComputePipelineDeferred(pipelineBuildDefinition);

	m_pipelineType = rhi::EPipelineType::Compute;
}

VkPipeline RHIPipeline::GetCachedPipelineHandle() const
{
	if (m_cachedPipelineHandle == VK_NULL_HANDLE)
	{
		CachePipelineHandle(VulkanRHI::GetPipelinesManager().GetPipelineHandle(m_pipelineID));
	}

	SPT_CHECK(!!m_cachedPipelineHandle);

	return m_cachedPipelineHandle;
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