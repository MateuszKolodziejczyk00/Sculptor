#pragma once

#include "RHIMacros.h"
#include "SculptorCoreTypes.h"
#include "RHICore/RHIPipelineTypes.h"
#include "RHICore/RHIPipelineDefinitionTypes.h"
#include "Vulkan/Debug/DebugUtils.h"


namespace spt::vulkan
{

struct RHI_API RHIPipelineReleaseTicket
{
	void ExecuteReleaseRHI();

	RHIResourceReleaseTicket<VkPipeline> handle;

#if SPT_RHI_DEBUG
	lib::HashedString name;
#endif // SPT_RHI_DEBUG
};


class RHI_API RHIPipeline
{
public:

	RHIPipeline();

	/** Initialize graphics pipeline */
	void InitializeRHI(const rhi::GraphicsPipelineShadersDefinition& shaderStagesDef, const rhi::GraphicsPipelineDefinition& pipelineDefinition);

	/** Initialize compute pipeline */
	void InitializeRHI(const rhi::RHIShaderModule& computeShaderModule);

	/** Initialize ray tracing pipeline */
	void InitializeRHI(const rhi::RayTracingShadersDefinition& shadersDef, const rhi::RayTracingPipelineDefinition& pipelineDef);

	void ReleaseRHI();

	RHIPipelineReleaseTicket DeferredReleaseRHI();

	SPT_NODISCARD Bool IsValid() const;

	SPT_NODISCARD rhi::EPipelineType GetPipelineType() const;

	SPT_NODISCARD rhi::PipelineStatistics GetPipelineStatistics() const;

	void						SetName(const lib::HashedString& name);
	const lib::HashedString&	GetName() const;

	// Vulkan ====================================================

	SPT_NODISCARD VkPipeline			GetHandle() const;

private:

	void InitializeGraphicsPipeline(const rhi::GraphicsPipelineShadersDefinition& shaderStagesDef, const rhi::GraphicsPipelineDefinition& pipelineDefinition);
	void InitializeComputePipeline(const rhi::RHIShaderModule& computeShaderModule);
	void InitializeRayTracingPipeline(const rhi::RayTracingShadersDefinition& shadersDef, const rhi::RayTracingPipelineDefinition& pipelineDef);
	
	VkPipeline						m_handle;

	rhi::EPipelineType				m_pipelineType;

	DebugName						m_debugName;
};

} // spt::vulkan
