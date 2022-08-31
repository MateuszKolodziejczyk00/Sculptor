#pragma once

#include "Vulkan/VulkanCore.h"
#include "SculptorCoreTypes.h"
#include "PipelineLayout.h"
#include "RHICore/RHIGraphicsPipelineTypes.h"


namespace spt::vulkan
{

struct PipelineBuildData
{
	PipelineBuildData()
		: id(idxNone<PipelineID>)
	{ }

	PipelineID id;
};


struct PipelinesBuildsBatch
{

public:

	static constexpr SizeType pipelinesBuildBatchMaxSize = 32;

	using PipelineBuildDataRef = std::tuple<VkGraphicsPipelineCreateInfo&, PipelineBuildData&>;

	PipelinesBuildsBatch();

	SPT_NODISCARD Bool					HasPendingBuilds() const;

	SPT_NODISCARD Int32					AcquireNewBuildIdx();

	SPT_NODISCARD PipelineBuildDataRef	GetPiplineCreateData(Int32 buildIdx);
	SPT_NODISCARD PipelineID			GetPipelineID(Int32 buildIdx) const;

	void GetPipelineCreateInfos(const VkGraphicsPipelineCreateInfo*& outPipelineInfos, Uint32& outPipelinesNum) const;

private:

	using PipelineCreateInfos	= lib::StaticArray<VkGraphicsPipelineCreateInfo, pipelinesBuildBatchMaxSize>;
	using PipelineBuildDatas	= lib::StaticArray<PipelineBuildData, pipelinesBuildBatchMaxSize>;
	
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

	SPT_NODISCARD PipelineID BuildPipelineDeferred(const rhi::PipelineShaderStagesDefinition& shaderStagesDef, const rhi::GraphicsPipelineDefinition& pipelineDefinition, PipelineLayout layout);

	SPT_NODISCARD VkPipeline GetPipelineHandle(PipelineID id) const;

private:

	void BuildBatchedPipelines_AssumesLocked();

	lib::HashMap<PipelineID, VkPipeline> m_cachedPipelines;

	PipelinesBuildsBatch m_currentBuildsBatch;
};

} // spt::vulkan
