#pragma once

#include "SculptorCoreTypes.h"


namespace spt::vulkan
{

template<typename TVulkanCreateInfoType, typename TBuildDataType, SizeType pipelinesBuildsBatchMaxSize>
class PipelinesBatchBuilder
{
public:

	using VulkanCreateInfoType	= TVulkanCreateInfoType;
	using BuildDataType			= TBuildDataType;

	PipelinesBatchBuilder()
		: m_currentBuildIdx(0)
	{ }

	SPT_NODISCARD Bool HasPendingBuilds() const;

	template<typename TBuildPipelineDataFunctionFunction>
	void AppendPipelineCreateData(const TBuildPipelineDataFunctionFunction& buildPipelineDataFunction, Bool& outShouldFlushPendingBuilds);

	template<typename TBuildPipelinesFunction>
	SPT_NODISCARD lib::DynamicArray<std::pair<PipelineID, VkPipeline>> BuildPendingPipelines(const TBuildPipelinesFunction& buildPipelinesFunction);

private:

	template<typename TBuildPipelineDataFunctionFunction>
	SPT_NODISCARD Bool							TryBuildPipelineCreateData_AssumesLockedShared(const TBuildPipelineDataFunctionFunction& buildPipelineDataFunction, Int32& outBuildIdx);

	SPT_NODISCARD Int32							AcquireNewBuildIdx_AssumesLockedShared();

	SPT_NODISCARD PipelineID					GetPipelineID(Int32 buildIdx) const;

	SPT_NODISCARD const VulkanCreateInfoType*	GetPipelineCreateInfos(Uint32& outPipelinesNum) const;

	void ResetPipelineBuildDatas();

	Bool ShouldFlushPipelineBuilds(Int32 buildIdx) const;

	using PipelineCreateInfos	= lib::StaticArray<VulkanCreateInfoType, pipelinesBuildsBatchMaxSize>;
	using PipelineBuildDatas	= lib::StaticArray<BuildDataType, pipelinesBuildsBatchMaxSize>;
	
	PipelineCreateInfos	m_pipelineCreateInfos;
	PipelineBuildDatas	m_pipelineBuildDatas;

	std::atomic<Int32>	m_currentBuildIdx;

	lib::SharedLock					m_pipelineBuildsLock;
	std::condition_variable_any		m_flushingPipelineBuilsCV;
};

template<typename TVulkanCreateInfoType, typename TBuildDataType, SizeType pipelinesBuildsBatchMaxSize>
Bool PipelinesBatchBuilder<TVulkanCreateInfoType, TBuildDataType, pipelinesBuildsBatchMaxSize>::HasPendingBuilds() const
{
	return m_currentBuildIdx.load(std::memory_order_acquire) > 0;
}

template<typename TVulkanCreateInfoType, typename TBuildDataType, SizeType pipelinesBuildsBatchMaxSize>
template<typename TBuildPipelineDataFunctionFunction>
void PipelinesBatchBuilder<TVulkanCreateInfoType, TBuildDataType, pipelinesBuildsBatchMaxSize>::AppendPipelineCreateData(const TBuildPipelineDataFunctionFunction& buildPipelineDataFunction, Bool& outShouldFlushPendingBuilds)
{
	Int32 buildIdx = idxNone<Int32>;

	{
		SPT_PROFILE_SCOPE("TryBuildPipelineCreateData");

		lib::SharedLockGuard pipelineBuildsLockGuard(m_pipelineBuildsLock);
		m_flushingPipelineBuilsCV.wait(pipelineBuildsLockGuard, [&, this]() { return TryBuildPipelineCreateData_AssumesLockedShared(buildPipelineDataFunction, buildIdx); });
	}

	SPT_CHECK(buildIdx != idxNone<Int32>);

	outShouldFlushPendingBuilds = ShouldFlushPipelineBuilds(buildIdx);
}

template<typename TVulkanCreateInfoType, typename TBuildDataType, SizeType pipelinesBuildsBatchMaxSize>
template<typename TBuildPipelinesFunction>
lib::DynamicArray<std::pair<PipelineID, VkPipeline>> PipelinesBatchBuilder<TVulkanCreateInfoType, TBuildDataType, pipelinesBuildsBatchMaxSize>::BuildPendingPipelines(const TBuildPipelinesFunction& buildPipelinesFunction)
{
	SPT_PROFILE_FUNCTION();

	// Guaranteed to have enough big size
	lib::StaticArray<VkPipeline, pipelinesBuildsBatchMaxSize> outPipelines;

	Uint32 pipelinesNum = 0;

	{
		const lib::UniqueLockGuard pipelineBuildsLockGuard(m_pipelineBuildsLock);

		const VulkanCreateInfoType* pipelineInfos = GetPipelineCreateInfos(pipelinesNum);

		if (pipelineInfos != nullptr && pipelinesNum > 0)
		{
			std::invoke(buildPipelinesFunction, pipelineInfos, pipelinesNum, outPipelines.data());
		}

		ResetPipelineBuildDatas();
	}

	m_flushingPipelineBuilsCV.notify_all();

	lib::DynamicArray<std::pair<PipelineID, VkPipeline>> createdPipelines;
	createdPipelines.reserve(static_cast<SizeType>(pipelinesNum));

	for (SizeType idx = 0; idx < pipelinesNum; ++idx)
	{
		createdPipelines.emplace_back(GetPipelineID(static_cast<Uint32>(idx)), outPipelines[idx]);
	}

	return createdPipelines;
}

template<typename TVulkanCreateInfoType, typename TBuildDataType, SizeType pipelinesBuildsBatchMaxSize>
template<typename TBuildPipelineDataFunctionFunction>
Bool PipelinesBatchBuilder<TVulkanCreateInfoType, TBuildDataType, pipelinesBuildsBatchMaxSize>::TryBuildPipelineCreateData_AssumesLockedShared(const TBuildPipelineDataFunctionFunction& buildPipelineDataFunction, Int32& outBuildIdx)
{
	SPT_PROFILE_FUNCTION();

	outBuildIdx = AcquireNewBuildIdx_AssumesLockedShared();
	const Bool success = (outBuildIdx != idxNone<Int32>);

	if (success)
	{

		BuildDataType& buildData			= m_pipelineBuildDatas[outBuildIdx];
		VulkanCreateInfoType& pipelineInfo	= m_pipelineCreateInfos[outBuildIdx];

		std::invoke(buildPipelineDataFunction, buildData, pipelineInfo);
	}

	return success;
}

template<typename TVulkanCreateInfoType, typename TBuildDataType, SizeType pipelinesBuildsBatchMaxSize>
Int32 PipelinesBatchBuilder<TVulkanCreateInfoType, TBuildDataType, pipelinesBuildsBatchMaxSize>::AcquireNewBuildIdx_AssumesLockedShared()
{
	const Int32 buildIdx = m_currentBuildIdx.fetch_add(1, std::memory_order_acq_rel);

	// check if acquired idx is in valid range
	return buildIdx < static_cast<Int32>(pipelinesBuildsBatchMaxSize) ? buildIdx : idxNone<Int32>;
}

template<typename TVulkanCreateInfoType, typename TBuildDataType, SizeType pipelinesBuildsBatchMaxSize>
PipelineID PipelinesBatchBuilder<TVulkanCreateInfoType, TBuildDataType, pipelinesBuildsBatchMaxSize>::GetPipelineID(Int32 buildIdx) const
{
	SPT_CHECK(buildIdx != idxNone<Int32>);

	return m_pipelineBuildDatas[buildIdx].pipelineID;
}

template<typename TVulkanCreateInfoType, typename TBuildDataType, SizeType pipelinesBuildsBatchMaxSize>
const TVulkanCreateInfoType* PipelinesBatchBuilder<TVulkanCreateInfoType, TBuildDataType, pipelinesBuildsBatchMaxSize>::GetPipelineCreateInfos(Uint32& outPipelinesNum) const
{
	const Uint32 pipelinesNum = std::min(static_cast<Uint32>(m_currentBuildIdx.load(std::memory_order_acquire)),
		static_cast<Uint32>(pipelinesBuildsBatchMaxSize));

	outPipelinesNum = pipelinesNum;
	return pipelinesNum > 0 ? m_pipelineCreateInfos.data() : nullptr;
}

template<typename TVulkanCreateInfoType, typename TBuildDataType, SizeType pipelinesBuildsBatchMaxSize>
void PipelinesBatchBuilder<TVulkanCreateInfoType, TBuildDataType, pipelinesBuildsBatchMaxSize>::ResetPipelineBuildDatas()
{
	m_currentBuildIdx.store(0, std::memory_order_release);
}

template<typename TVulkanCreateInfoType, typename TBuildDataType, SizeType pipelinesBuildsBatchMaxSize>
Bool PipelinesBatchBuilder<TVulkanCreateInfoType, TBuildDataType, pipelinesBuildsBatchMaxSize>::ShouldFlushPipelineBuilds(Int32 buildIdx) const
{
	// return true only for thread that was using last available idx
	return buildIdx == pipelinesBuildsBatchMaxSize - 1;
}

} // spt::vulkan
