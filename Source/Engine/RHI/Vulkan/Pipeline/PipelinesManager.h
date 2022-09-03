#pragma once

#include "Vulkan/VulkanCore.h"
#include "SculptorCoreTypes.h"
#include "PipelineLayout.h"
#include "RHICore/RHIPipelineDefinitionTypes.h"


namespace spt::vulkan
{

struct PipelineBuildDefinition
{
	PipelineBuildDefinition(const rhi::PipelineShaderStagesDefinition&	inShaderStagesDef,
							const rhi::GraphicsPipelineDefinition&		inPipelineDefinition,
							PipelineLayout								inLayout)
		: shaderStagesDef(inShaderStagesDef)
		, pipelineDefinition(inPipelineDefinition)
		, layout(inLayout)
	{ }
	
	const rhi::PipelineShaderStagesDefinition&	shaderStagesDef;
	const rhi::GraphicsPipelineDefinition&		pipelineDefinition;
	PipelineLayout								layout;
};


struct GraphicsPipelineBuildData
{
	GraphicsPipelineBuildData()
		: pipelineID(idxNone<PipelineID>)
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


struct ComputePipelineBuildData
{
	ComputePipelineBuildData()
		: pipelineID(idxNone<PipelineID>)
	{ }

	PipelineID							pipelineID;
    
	VkPipelineLayout					pipelineLayout;
};


struct PipelinesBuildsBatch
{

public:

	static constexpr SizeType pipelinesBuildBatchMaxSize = 32;

	using BatchedPipelineBuildRef = std::tuple<VkGraphicsPipelineCreateInfo&, GraphicsPipelineBuildData&>;

	PipelinesBuildsBatch();

	SPT_NODISCARD Bool	HasPendingBuilds() const;

	SPT_NODISCARD Int32	AcquireNewBuildIdx();

	SPT_NODISCARD BatchedPipelineBuildRef	GetPiplineCreateData(Int32 buildIdx);
	SPT_NODISCARD PipelineID				GetPipelineID(Int32 buildIdx) const;

	Bool ShouldFlushPipelineBuilds(Int32 buildIdx) const;

	void GetPipelineCreateInfos(const VkGraphicsPipelineCreateInfo*& outPipelineInfos, Uint32& outPipelinesNum) const;
	void ResetPipelineBuildDatas();

private:

	using PipelineCreateInfos	= lib::StaticArray<VkGraphicsPipelineCreateInfo, pipelinesBuildBatchMaxSize>;
	using PipelineBuildDatas	= lib::StaticArray<GraphicsPipelineBuildData, pipelinesBuildBatchMaxSize>;
	
	PipelineCreateInfos	m_pipelineCreateInfos;
	PipelineBuildDatas	m_pipelineBuildDatas;

	std::atomic<Int32>	m_currentBuildIdx;
};


class PipelinesManager
{
public:

	PipelinesManager();

	void InitializeRHI();
	void ReleaseRHI();

	SPT_NODISCARD PipelineID BuildPipelineDeferred(const PipelineBuildDefinition& pipelineBuildDef);

	SPT_NODISCARD VkPipeline GetPipelineHandle(PipelineID pipelineID) const;

private:

	Bool TryBuildPipelineCreateData_AssumesLockedShared(const PipelineBuildDefinition& pipelineBuildDef, PipelineID pipelineID, Int32& outBuildIdx);
	Int32 AcquireNewBuildIdx_AssumesLockedShared();
	void BuildPipelineCreateData(const PipelineBuildDefinition& pipelineBuildDef, PipelineID pipelineID, Int32 buildIdx);

	Bool ShouldFlushPipelineBuilds(Int32 buildIdx) const;

	void BuildBatchedPipelines_AssumesLocked();

	lib::HashMap<PipelineID, VkPipeline> m_cachedPipelines;

	PipelinesBuildsBatch			m_currentBuildsBatch;
	lib::SharedLock					m_pipelineBuildsLock;
	std::condition_variable_any		m_flushingPipelineBuilsCV;

	std::atomic<PipelineID>	m_piplineIDCounter;
};

} // spt::vulkan
