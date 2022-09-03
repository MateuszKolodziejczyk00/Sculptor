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
	void						InitializeRHI(const rhi::PipelineShaderStagesDefinition& shaderStagesDef, const rhi::GraphicsPipelineDefinition& pipelineDefinition, const rhi::PipelineLayoutDefinition& layoutDefinition);

	/** Initialize compute pipeline */
	void						InitializeRHI(const rhi::RHIShaderModule& computeShaderModule, const rhi::PipelineLayoutDefinition& layoutDefinition);

	void						ReleaseRHI();

	SPT_NODISCARD Bool			IsValid() const;

	rhi::EPipelineType			GetPipelineType() const;

	void						SetName(const lib::HashedString& name);
	const lib::HashedString&	GetName() const;

	// Vulkan ====================================================

	SPT_NODISCARD VkPipeline	GetHandle() const;

private:

	void InitializeGraphicsPipeline(const rhi::PipelineShaderStagesDefinition& shaderStagesDef, const rhi::GraphicsPipelineDefinition& pipelineDefinition, const PipelineLayout& layout);
	void InitializeComputePipeline(const rhi::RHIShaderModule& computeShaderModule, const PipelineLayout& layout);

	void CachePipelineHandle(VkPipeline pipelineHandle) const;

	PipelineID			m_pipelineID;
	mutable VkPipeline	m_cachedPipelineHandle;

	PipelineLayout		m_layout;

	rhi::EPipelineType	m_pipelineType;

	DebugName			m_debugName;
};

} // spt::vulkan