#pragma once

#include "Vulkan/VulkanCore.h"
#include "SculptorCoreTypes.h"
#include "PipelineLayout.h"
#include "RHICore/RHIPipelineDefinitionTypes.h"


namespace spt::vulkan
{

struct GraphicsPipelineBuildDefinition
{
	GraphicsPipelineBuildDefinition(const rhi::PipelineShaderStagesDefinition&	inShaderStagesDef,
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
	ComputePipelineBuildDefinition(const RHIShaderModule& inComputeShaderModule, PipelineLayout inPipelineLayout)
		: computeShaderModule(inComputeShaderModule)
		, layout(inPipelineLayout)
	{ }

	RHIShaderModule		computeShaderModule;
	PipelineLayout		layout;
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


template<typename TVulkanCreateInfoType, typename TBuildDataType>
class ThreadsafePipelineBuildsBatcher
{
public:

	static constexpr SizeType pipelinesBuildBatchMaxSize = 32;

	using VulkanCreateInfoType	= TVulkanCreateInfoType;
	using BuildDataType			= TBuildDataType;

	using BatchedPipelineBuildRef = std::tuple<VulkanCreateInfoType&, BuildDataType&>;

	ThreadsafePipelineBuildsBatcher()
		: m_currentBuildIdx(0)
	{ }

	SPT_NODISCARD Bool HasPendingBuilds() const
	{
		return m_currentBuildIdx.load(std::memory_order_acquire) > 0;
	}

	template<typename TBuildFunction>
	void BuildPipelineCreateData(const TBuildFunction& buildFunction, Bool& outShouldFlushPendingBuilds)
	{
		Int32 buildIdx = idxNone<Int32>;

		{
			SPT_PROFILE_SCOPE("TryBuildPipelineCreateData");

			lib::SharedLockGuard pipelineBuildsLockGuard(m_pipelineBuildsLock);
			m_flushingPipelineBuilsCV.wait(pipelineBuildsLockGuard, [&, this]() { return TryBuildPipelineCreateData_AssumesLockedShared(buildFunction, buildIdx); });
		}

		SPT_CHECK(buildIdx != idxNone<Int32>);
	
		outShouldFlushPendingBuilds = ShouldFlushPipelineBuilds(buildIdx);
	}

	template<typename TBuildPipelinesFunction>
	lib::DynamicArray<std::pair<PipelineID, VkPipeline>> BuildPipelines(const TBuildPipelinesFunction& buildPipelinesFunction)
	{
		SPT_PROFILE_FUNCTION();

		const lib::UniqueLockGuard pipelineBuildsLockGuard(m_pipelineBuildsLock);

		Uint32 pipelinesNum = 0;
		const VulkanCreateInfoType* pipelineInfos = GetPipelineCreateInfos(pipelinesNum);

		lib::DynamicArray<std::pair<PipelineID, VkPipeline>> createdPipelines;
		createdPipelines.reserve(static_cast<SizeType>(pipelinesNum));

		if (pipelineInfos != nullptr && pipelinesNum > 0)
		{
			// Guaranteed to have enough big size
			lib::StaticArray<VkPipeline, pipelinesBuildBatchMaxSize> outPipelines;

			std::invoke(buildPipelinesFunction, pipelineInfos, pipelinesNum, outPipelines.data());

			for (SizeType idx = 0; idx < pipelinesNum; ++idx)
			{
				createdPipelines.emplace_back(GetPipelineID(static_cast<Uint32>(idx)), outPipelines[idx]);
			}
		}

		m_flushingPipelineBuilsCV.notify_all();

		return createdPipelines;
	}

private:

	template<typename TBuildFunction>
	Bool TryBuildPipelineCreateData_AssumesLockedShared(const TBuildFunction& buildFunction, Int32& outBuildIdx)
	{
		SPT_PROFILE_FUNCTION();

		outBuildIdx = AcquireNewBuildIdx();
		const Bool success = (outBuildIdx != idxNone<Int32>);

		if (success)
		{
			BatchedPipelineBuildRef batchedBuildRef	= GetPiplineCreateData(outBuildIdx);

			BuildDataType& buildData			= std::get<BuildDataType&>(batchedBuildRef);
			VulkanCreateInfoType& pipelineInfo	= std::get<VulkanCreateInfoType&>(batchedBuildRef);

			std::invoke(buildFunction, buildData, pipelineInfo);
		}

		return success;
	}

	SPT_NODISCARD Int32 AcquireNewBuildIdx()
	{
		const Int32 buildIdx = m_currentBuildIdx.fetch_add(1, std::memory_order_acq_rel);

		// check if acquired idx is in valid range
		return buildIdx < static_cast<Int32>(pipelinesBuildBatchMaxSize) ? buildIdx : idxNone<Int32>;
	}

	SPT_NODISCARD PipelineID GetPipelineID(Int32 buildIdx) const
	{
		SPT_CHECK(buildIdx != idxNone<Int32>);

		return m_pipelineBuildDatas[buildIdx].pipelineID;
	}

	SPT_NODISCARD BatchedPipelineBuildRef GetPiplineCreateData(Int32 buildIdx)
	{
		SPT_CHECK(buildIdx != idxNone<Int32>);

		return std::tie(m_pipelineCreateInfos[buildIdx], m_pipelineBuildDatas[buildIdx]);
	}

	const VulkanCreateInfoType* GetPipelineCreateInfos(Uint32& outPipelinesNum) const
	{
		const Uint32 pipelinesNum = std::min(	static_cast<Uint32>(m_currentBuildIdx.load(std::memory_order_acquire)),
												static_cast<Uint32>(pipelinesBuildBatchMaxSize));
		
		outPipelinesNum = pipelinesNum;
		return pipelinesNum > 0 ? m_pipelineCreateInfos.data() : nullptr;
	}

	void ResetPipelineBuildDatas()
	{
		m_currentBuildIdx.store(0, std::memory_order_release);
	}

	Bool ShouldFlushPipelineBuilds(Int32 buildIdx) const
	{
		// return true only for thread that was using last available idx
		return buildIdx == pipelinesBuildBatchMaxSize - 1;
	}

	using PipelineCreateInfos	= lib::StaticArray<VulkanCreateInfoType, pipelinesBuildBatchMaxSize>;
	using PipelineBuildDatas	= lib::StaticArray<BuildDataType, pipelinesBuildBatchMaxSize>;
	
	PipelineCreateInfos	m_pipelineCreateInfos;
	PipelineBuildDatas	m_pipelineBuildDatas;

	std::atomic<Int32>	m_currentBuildIdx;

	lib::SharedLock					m_pipelineBuildsLock;
	std::condition_variable_any		m_flushingPipelineBuilsCV;
};


using ThreadsafeGraphicsPipelineBuildsBatcher = ThreadsafePipelineBuildsBatcher<VkGraphicsPipelineCreateInfo, GraphicsPipelineBuildData>;
using ThreadsafeComputePipelineBuildsBatcher = ThreadsafePipelineBuildsBatcher<VkComputePipelineCreateInfo, ComputePipelineBuildData>;


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

	SPT_NODISCARD VkPipeline	GetPipelineHandle(PipelineID pipelineID) const;

private:

	lib::HashMap<PipelineID, VkPipeline> m_cachedPipelines;

	ThreadsafeGraphicsPipelineBuildsBatcher	m_graphicsPipelineBuildsBatcher;
	ThreadsafeComputePipelineBuildsBatcher	m_computePipelineBuildsBatcher;

	std::atomic<PipelineID>	m_piplineIDCounter;
};

} // spt::vulkan
