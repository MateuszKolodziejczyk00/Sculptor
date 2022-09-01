#include "PipelinesManager.h"
#include "Vulkan/VulkanRHI.h"
#include "Vulkan/RHIToVulkanCommon.h"

namespace spt::vulkan
{

namespace priv
{

static void BuildShaderInfos(const PipelineBuildDefinition& pipelineBuildDef, lib::DynamicArray<VkPipelineShaderStageCreateInfo>& outShaderStageInfos)
{
	SPT_PROFILE_FUNCTION();

	const lib::DynamicArray<RHIShaderModule>& shaderModules = pipelineBuildDef.shaderStagesDef.shaderModules;
	std::transform(	std::cbegin(shaderModules), std::cend(shaderModules),
					std::back_inserter(outShaderStageInfos),
					[](const rhi::RHIShaderModule& shaderModule) -> VkPipelineShaderStageCreateInfo
					{
						SPT_CHECK(shaderModule.IsValid());

						VkPipelineShaderStageCreateInfo stageInfo{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
						stageInfo.stage = RHIToVulkan::GetShaderStage(shaderModule.GetStage());
						stageInfo.module = shaderModule.GetHandle();
						stageInfo.pName = "main";

						return stageInfo;
					});
}

} // priv

//////////////////////////////////////////////////////////////////////////////////////////////////
// PipelinesBuildsBatch ==========================================================================

PipelinesBuildsBatch::PipelinesBuildsBatch()
	: m_currentBuildIdx(0)
{ }

Bool PipelinesBuildsBatch::HasPendingBuilds() const
{
	return m_currentBuildIdx.load(std::memory_order_acquire) > 0;
}

Int32 PipelinesBuildsBatch::AcquireNewBuildIdx()
{
	const Int32 buildIdx = m_currentBuildIdx.fetch_add(1, std::memory_order_release);

	// check if acquired idx is in valid range
	return buildIdx < static_cast<Int32>(pipelinesBuildBatchMaxSize) ? buildIdx : idxNone<Int32>;
}

PipelinesBuildsBatch::BatchedPipelineBuildRef PipelinesBuildsBatch::GetPiplineCreateData(Int32 buildIdx)
{
	SPT_CHECK(buildIdx != idxNone<Int32>);

	return std::tie(m_pipelineCreateInfos[buildIdx], m_pipelineBuildDatas[buildIdx]);
}

PipelineID PipelinesBuildsBatch::GetPipelineID(Int32 buildIdx) const
{
	SPT_CHECK(buildIdx != idxNone<Int32>);

	return m_pipelineBuildDatas[buildIdx].id;
}

void PipelinesBuildsBatch::GetPipelineCreateInfos(const VkGraphicsPipelineCreateInfo*& outPipelineInfos, Uint32& outPipelinesNum) const
{
	const Uint32 pipelinesNum = std::min(	static_cast<Uint32>(m_currentBuildIdx.load(std::memory_order_acquire)),
											static_cast<Uint32>(pipelinesBuildBatchMaxSize));
	
	outPipelineInfos = pipelinesNum > 0 ? m_pipelineCreateInfos.data() : nullptr;

}

//////////////////////////////////////////////////////////////////////////////////////////////////
// PipelinesManager ==============================================================================

PipelinesManager::PipelinesManager()
{ }

void PipelinesManager::InitializeRHI()
{
	// Nothing for now
}

void PipelinesManager::ReleaseRHI()
{
	SPT_PROFILE_FUNCTION();

	std::for_each(std::cbegin(m_cachedPipelines), std::cend(m_cachedPipelines),
		[](const auto& idToPipeline)
		{
			const VkPipeline pipeline = idToPipeline.second;
			SPT_CHECK(pipeline != VK_NULL_HANDLE);
			vkDestroyPipeline(VulkanRHI::GetDeviceHandle(), pipeline, VulkanRHI::GetAllocationCallbacks());
		});

	m_cachedPipelines.clear();
}

PipelineID PipelinesManager::BuildPipelineDeferred(const PipelineBuildDefinition& pipelineBuildDef)
{
	SPT_PROFILE_FUNCTION();

	const Int32 buildIdx = m_currentBuildsBatch.AcquireNewBuildIdx();
	if (buildIdx == idxNone<Int32>)
	{
		// flush
	}

	PipelinesBuildsBatch::BatchedPipelineBuildRef batchedBuildRef	= m_currentBuildsBatch.GetPiplineCreateData(buildIdx);
	PipelineBuildData& buildData									= std::get<PipelineBuildData&>(batchedBuildRef);
	//VkGraphicsPipelineCreateInfo& pipelineInfo						= std::get<VkGraphicsPipelineCreateInfo&>(batchedBuildRef);

	priv::BuildShaderInfos(pipelineBuildDef, buildData.shaderStages);

	return 0;
}

VkPipeline PipelinesManager::GetPipelineHandle(PipelineID id) const
{
	SPT_PROFILE_FUNCTION();

	if (id == idxNone<PipelineID>)
	{
		return VK_NULL_HANDLE;
	}

	SPT_CHECK(!m_currentBuildsBatch.HasPendingBuilds());

	const auto pipelineHandle = m_cachedPipelines.find(id);
	return pipelineHandle != std::cend(m_cachedPipelines) ? pipelineHandle->second : VK_NULL_HANDLE;
}

void PipelinesManager::BuildBatchedPipelines_AssumesLocked()
{
	SPT_PROFILE_FUNCTION();

	const VkGraphicsPipelineCreateInfo* pipelineInfos = nullptr;
	Uint32 pipelinesNum = 0;
	m_currentBuildsBatch.GetPipelineCreateInfos(pipelineInfos, pipelinesNum);

	if (pipelineInfos != nullptr && pipelinesNum > 0)
	{
		const VkPipelineCache pipelineCache = VK_NULL_HANDLE;

		lib::StaticArray<VkPipeline, PipelinesBuildsBatch::pipelinesBuildBatchMaxSize> pipelines;
		SPT_VK_CHECK(vkCreateGraphicsPipelines(	VulkanRHI::GetDeviceHandle(),
												pipelineCache,
												pipelinesNum,
												pipelineInfos,
												VulkanRHI::GetAllocationCallbacks(),
												pipelines.data()));

		for (Uint32 idx = 0; idx < pipelinesNum; ++idx)
		{
			const PipelineID id = m_currentBuildsBatch.GetPipelineID(static_cast<Int32>(idx));
			const VkPipeline pipelineHandle = pipelines[idx];

			m_cachedPipelines.emplace(id, pipelineHandle);
		}
	}
}

} // spt::vulkan
