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

	void						InitializeRHI(const rhi::PipelineShaderStagesDefinition& shaderStagesDef, const rhi::GraphicsPipelineDefinition& pipelineDefinition, const rhi::PipelineLayoutDefinition& layoutDefinition);
	void						ReleaseRHI();

	Bool						IsValid() const;

	void						SetName(const lib::HashedString& name);
	const lib::HashedString&	GetName() const;

	// Vulkan ====================================================

	VkPipeline					GetHandle() const;

private:

	VkPipeline		m_pipelineHandle;

	PipelineLayout	m_layout;

	DebugName		m_debugName;
};

} // spt::vulkan