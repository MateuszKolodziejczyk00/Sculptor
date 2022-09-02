#include "PipelinesManager.h"
#include "Vulkan/VulkanRHI.h"
#include "Vulkan/RHIToVulkanCommon.h"

namespace spt::vulkan
{

namespace helpers
{

static void BuildShaderInfos(const PipelineBuildDefinition& pipelineBuildDef, lib::DynamicArray<VkPipelineShaderStageCreateInfo>& outShaderStageInfos)
{
	SPT_PROFILE_FUNCTION();

	outShaderStageInfos.clear();

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

static void BuildInputAssemblyInfo(const PipelineBuildDefinition& pipelineBuildDef, VkPipelineInputAssemblyStateCreateInfo& inputAssemblyStateInfo)
{
	SPT_PROFILE_FUNCTION();
	
	inputAssemblyStateInfo = VkPipelineInputAssemblyStateCreateInfo{ VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
    inputAssemblyStateInfo.topology					= RHIToVulkan::GetPrimitiveTopology(pipelineBuildDef.pipelineDefinition.primitiveTopology);
    inputAssemblyStateInfo.primitiveRestartEnable	= VK_FALSE;
}

static void BuildRasterizationStateInfo(const PipelineBuildDefinition& pipelineBuildDef, VkPipelineRasterizationStateCreateInfo& rasterizationState)
{
	SPT_PROFILE_FUNCTION();

	const rhi::PipelineRasterizationDefinition& rasterizationDefinition = pipelineBuildDef.pipelineDefinition.rasterizationDefinition;

	rasterizationState = VkPipelineRasterizationStateCreateInfo{ VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
    rasterizationState.depthClampEnable			= VK_FALSE;
    rasterizationState.rasterizerDiscardEnable	= VK_FALSE;
	rasterizationState.polygonMode				= RHIToVulkan::GetPolygonMode(rasterizationDefinition.polygonMode);
	rasterizationState.cullMode					= RHIToVulkan::GetCullMode(rasterizationDefinition.cullMode);
    rasterizationState.frontFace				= VK_FRONT_FACE_CLOCKWISE;
    rasterizationState.lineWidth				= 1.f;
}

static void BuildMultisampleStateInfo(const PipelineBuildDefinition& pipelineBuildDef, VkPipelineMultisampleStateCreateInfo& multisampleState)
{
	SPT_PROFILE_FUNCTION();

	const rhi::MultisamplingDefinition& multisampleStateDefinition = pipelineBuildDef.pipelineDefinition.multisamplingDefinition;

	multisampleState = VkPipelineMultisampleStateCreateInfo{ VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
    multisampleState.rasterizationSamples = RHIToVulkan::GetSampleCount(multisampleStateDefinition.samplesNum);
}

static void BuildDepthStencilStateInfo(const PipelineBuildDefinition& pipelineBuildDef, VkPipelineDepthStencilStateCreateInfo& depthStencilState)
{
	SPT_CHECK_NO_ENTRY();

	const rhi::PipelineRenderTargetsDefinition& renderTargetsDefinition = pipelineBuildDef.pipelineDefinition.renderTargetsDefinition;
	const rhi::DepthRenderTargetDefinition& depthRTDef = renderTargetsDefinition.depthRTDefinition;

	depthStencilState = VkPipelineDepthStencilStateCreateInfo{ VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
    depthStencilState.depthTestEnable = depthRTDef.depthCompareOp != rhi::EDepthCompareOperation::None;
	if (depthRTDef.enableDepthWrite)
	{
		depthStencilState.depthWriteEnable	= VK_TRUE;
		depthStencilState.depthCompareOp	= RHIToVulkan::GetCompareOp(depthRTDef.depthCompareOp);
	}
	else
	{
		depthStencilState.depthWriteEnable = VK_FALSE;
	}
}

static void SetVulkanBlendType(rhi::ERenderTargetBlendType blendType, VkBlendFactor& outSrcBlendFactor, VkBlendFactor& outDstBlendFactor, VkBlendOp& outBlendOp)
{
	switch (blendType)
	{
	case rhi::ERenderTargetBlendType::Copy:
		outSrcBlendFactor = VK_BLEND_FACTOR_ONE;
		outDstBlendFactor = VK_BLEND_FACTOR_ZERO;
		outBlendOp = VK_BLEND_OP_ADD;
		break;
	
	case rhi::ERenderTargetBlendType::Add:
		outSrcBlendFactor = VK_BLEND_FACTOR_ONE;
		outDstBlendFactor = VK_BLEND_FACTOR_ONE;
		outBlendOp = VK_BLEND_OP_ADD;
		break;
	
	default:

		SPT_CHECK_NO_ENTRY();
		break;
	}
}

static void BuildColorBlendStateInfo(const PipelineBuildDefinition& pipelineBuildDef, lib::DynamicArray<VkPipelineColorBlendAttachmentState>& blendAttachmentStates, VkPipelineColorBlendStateCreateInfo& colorBlendState)
{
	SPT_CHECK_NO_ENTRY();

	blendAttachmentStates.clear();

	const rhi::PipelineRenderTargetsDefinition& renderTargetsDefinition = pipelineBuildDef.pipelineDefinition.renderTargetsDefinition;
	const lib::DynamicArray<rhi::ColorRenderTargetDefinition>& colorRTsDefinition = renderTargetsDefinition.colorRTsDefinition;

	std::transform(std::cbegin(colorRTsDefinition), std::cend(colorRTsDefinition),
		std::back_inserter(blendAttachmentStates),
		[](const rhi::ColorRenderTargetDefinition& colorRTDefinition)
		{
			VkPipelineColorBlendAttachmentState attachmentBlendState{};
			attachmentBlendState.blendEnable = VK_TRUE;
			SetVulkanBlendType(colorRTDefinition.colorBlendType, attachmentBlendState.srcColorBlendFactor, attachmentBlendState.dstColorBlendFactor, attachmentBlendState.colorBlendOp);
			SetVulkanBlendType(colorRTDefinition.alphaBlendType, attachmentBlendState.srcAlphaBlendFactor, attachmentBlendState.dstAlphaBlendFactor, attachmentBlendState.alphaBlendOp);
			attachmentBlendState.colorWriteMask = RHIToVulkan::GetColorComponentFlags(colorRTDefinition.colorWriteMask);;

			return attachmentBlendState;
		});

	colorBlendState = VkPipelineColorBlendStateCreateInfo{ VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
    colorBlendState.logicOpEnable		= VK_FALSE;
    colorBlendState.attachmentCount		= static_cast<Uint32>(blendAttachmentStates.size());
    colorBlendState.pAttachments		= blendAttachmentStates.data();
	colorBlendState.blendConstants[0]	= 0.f;
	colorBlendState.blendConstants[1]	= 0.f;
	colorBlendState.blendConstants[2]	= 0.f;
	colorBlendState.blendConstants[3]	= 0.f;
}

} // helpers

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

	helpers::BuildShaderInfos(pipelineBuildDef, buildData.shaderStages);
	helpers::BuildInputAssemblyInfo(pipelineBuildDef, buildData.inputAssemblyState);
	helpers::BuildRasterizationStateInfo(pipelineBuildDef, buildData.rasterizationState);
	helpers::BuildMultisampleStateInfo(pipelineBuildDef, buildData.multisampleState);
	helpers::BuildDepthStencilStateInfo(pipelineBuildDef, buildData.depthStencilState);
	helpers::BuildColorBlendStateInfo(pipelineBuildDef, buildData.blendAttachmentStates, buildData.colorBlendState);

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
