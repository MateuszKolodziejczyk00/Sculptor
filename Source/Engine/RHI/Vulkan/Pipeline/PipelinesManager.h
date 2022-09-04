#pragma once

#include "Vulkan/VulkanCore.h"
#include "SculptorCoreTypes.h"
#include "PipelinesBatchBuilder.h"
#include "PipelineLayout.h"
#include "RHICore/RHIPipelineDefinitionTypes.h"


namespace spt::vulkan
{

struct GraphicsPipelineBuildDefinition
{
	GraphicsPipelineBuildDefinition(const rhi::PipelineShaderStagesDefinition&	inShaderStagesDef,
									const rhi::GraphicsPipelineDefinition&		inPipelineDefinition,
									const PipelineLayout&						inLayout)
		: shaderStagesDef(inShaderStagesDef)
		, pipelineDefinition(inPipelineDefinition)
		, layout(inLayout)
	{ }
	
	const rhi::PipelineShaderStagesDefinition&	shaderStagesDef;
	const rhi::GraphicsPipelineDefinition&		pipelineDefinition;
	const PipelineLayout&						layout;
};


struct GraphicsPipelineBuildData
{
	GraphicsPipelineBuildData()
		: pipelineID(idxNone<PipelineID>)
		, pipelineLayout(VK_NULL_HANDLE)
	{ }

	PipelineID												pipelineID;
	
	lib::DynamicArray<VkPipelineShaderStageCreateInfo>		shaderStages;
    
	VkPipelineInputAssemblyStateCreateInfo					inputAssemblyStateInfo;
    
	VkPipelineRasterizationStateCreateInfo					rasterizationStateInfo;
    
	VkPipelineMultisampleStateCreateInfo					multisampleStateInfo;
    
	VkPipelineDepthStencilStateCreateInfo					depthStencilStateInfo;
	
	lib::DynamicArray<VkPipelineColorBlendAttachmentState>	blendAttachmentStates;
    VkPipelineColorBlendStateCreateInfo						colorBlendStateInfo;
    
	lib::DynamicArray<VkDynamicState>						dynamicStates;
	VkPipelineDynamicStateCreateInfo						dynamicStateInfo;

	lib::DynamicArray<VkFormat>								colorRTFormats;
	VkPipelineRenderingCreateInfo							pipelineRenderingInfo;
    
	VkPipelineLayout										pipelineLayout;
};


struct ComputePipelineBuildDefinition
{
	ComputePipelineBuildDefinition(const RHIShaderModule& inComputeShaderModule, const PipelineLayout& inPipelineLayout)
		: computeShaderModule(inComputeShaderModule)
		, layout(inPipelineLayout)
	{ }

	const RHIShaderModule&		computeShaderModule;
	const PipelineLayout&		layout;
};


struct ComputePipelineBuildData
{
	ComputePipelineBuildData()
		: pipelineID(idxNone<PipelineID>)
		, pipelineLayout(VK_NULL_HANDLE)
	{ }

	PipelineID							pipelineID;
    
	VkPipelineLayout					pipelineLayout;
};

static constexpr SizeType pipelinesBuildsBatchMaxSize = 32;
using GraphicsPipelinesBatchBuilder = PipelinesBatchBuilder<VkGraphicsPipelineCreateInfo, GraphicsPipelineBuildData, pipelinesBuildsBatchMaxSize>;
using ComputePipelinesBatchBuilder = PipelinesBatchBuilder<VkComputePipelineCreateInfo, ComputePipelineBuildData, pipelinesBuildsBatchMaxSize>;


class PipelinesManager
{
public:

	PipelinesManager();

	void InitializeRHI();
	void ReleaseRHI();

	SPT_NODISCARD PipelineID	BuildGraphicsPipelineDeferred(const GraphicsPipelineBuildDefinition& pipelineBuildDef);
	void						FlushPendingGraphicsPipelines();

	SPT_NODISCARD PipelineID	BuildComputePipelineDeferred(const ComputePipelineBuildDefinition& pipelineBuildDef);
	void						FlushPendingComputePipelines();

	void						ReleasePipeline(PipelineID pipelineID);

	SPT_NODISCARD VkPipeline	GetPipelineHandle(PipelineID pipelineID) const;

private:

	void CacheCreatedPipelines(const lib::DynamicArray<std::pair<PipelineID, VkPipeline>>& createdPipelines);

	lib::HashMap<PipelineID, VkPipeline> m_cachedPipelines;

	GraphicsPipelinesBatchBuilder	m_graphicsPipelinesBatchBuilder;
	ComputePipelinesBatchBuilder	m_computePipelinesBatchBuilder;

	std::atomic<PipelineID>	m_piplineIDCounter;
};

} // spt::vulkan
