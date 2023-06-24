#pragma once

#include "RHIMacros.h"
#include "SculptorCoreTypes.h"
#include "RHICore/RHIPipelineTypes.h"
#include "RHICore/RHIPipelineDefinitionTypes.h"
#include "Vulkan/Pipeline/PipelineLayout.h"
#include "Vulkan/Debug/DebugUtils.h"


namespace spt::vulkan
{

class RHI_API RHIPipeline
{
public:

	RHIPipeline();

	/** Initialize graphics pipeline */
	void InitializeRHI(const rhi::GraphicsPipelineShadersDefinition& shaderStagesDef, const rhi::GraphicsPipelineDefinition& pipelineDefinition, const rhi::PipelineLayoutDefinition& layoutDefinition);

	/** Initialize compute pipeline */
	void InitializeRHI(const rhi::RHIShaderModule& computeShaderModule, const rhi::PipelineLayoutDefinition& layoutDefinition);

	/** Initialize ray tracing pipeline */
	void InitializeRHI(const rhi::RayTracingShadersDefinition& shadersDef, const rhi::RayTracingPipelineDefinition& pipelineDef, const rhi::PipelineLayoutDefinition& layoutDefinition);

	void ReleaseRHI();

	SPT_NODISCARD Bool IsValid() const;

	SPT_NODISCARD rhi::EPipelineType GetPipelineType() const;

	SPT_NODISCARD rhi::DescriptorSetLayoutID GetDescriptorSetLayoutID(Uint32 dsIdx) const;

	void						SetName(const lib::HashedString& name);
	const lib::HashedString&	GetName() const;

	// Vulkan ====================================================

	SPT_NODISCARD VkPipeline			GetHandle() const;

	/** Can be called only on valid pipeline */
	SPT_NODISCARD const PipelineLayout&	GetPipelineLayout() const;

private:

	void InitializePipelineLayout(const rhi::PipelineLayoutDefinition& layoutDefinition);

	void InitializeGraphicsPipeline(const rhi::GraphicsPipelineShadersDefinition& shaderStagesDef, const rhi::GraphicsPipelineDefinition& pipelineDefinition, const PipelineLayout& layout);
	void InitializeComputePipeline(const rhi::RHIShaderModule& computeShaderModule, const PipelineLayout& layout);
	void InitializeRayTracingPipeline(const rhi::RayTracingShadersDefinition& shadersDef, const rhi::RayTracingPipelineDefinition& pipelineDef, const PipelineLayout& layout);
	
	VkPipeline						m_handle;

	lib::SharedPtr<PipelineLayout>	m_layout;

	rhi::EPipelineType				m_pipelineType;

	DebugName						m_debugName;
};

} // spt::vulkan