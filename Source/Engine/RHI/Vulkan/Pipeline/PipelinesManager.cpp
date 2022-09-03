#include "PipelinesManager.h"
#include "Vulkan/VulkanRHI.h"
#include "Vulkan/RHIToVulkanCommon.h"
#include "Vulkan/VulkanUtils.h"

namespace spt::vulkan
{

namespace helpers
{

static VkPipelineShaderStageCreateInfo BuildPipelineShaderStageInfo(const rhi::RHIShaderModule& shaderModule)
{
	SPT_CHECK(shaderModule.IsValid());

	VkPipelineShaderStageCreateInfo stageInfo{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
	stageInfo.stage = RHIToVulkan::GetShaderStage(shaderModule.GetStage());
	stageInfo.module = shaderModule.GetHandle();
	stageInfo.pName = "main";

	return stageInfo;
}

namespace gfx
{

static void BuildShaderInfos(const PipelineBuildDefinition& pipelineBuildDef, lib::DynamicArray<VkPipelineShaderStageCreateInfo>& outShaderStageInfos)
{
	SPT_PROFILE_FUNCTION();

	outShaderStageInfos.clear();

	const lib::DynamicArray<RHIShaderModule>& shaderModules = pipelineBuildDef.shaderStagesDef.shaderModules;

	std::transform(	std::cbegin(shaderModules), std::cend(shaderModules),
					std::back_inserter(outShaderStageInfos),
					&BuildPipelineShaderStageInfo);
}

static void BuildInputAssemblyInfo(const PipelineBuildDefinition& pipelineBuildDef, VkPipelineInputAssemblyStateCreateInfo& outInputAssemblyStateInfo)
{
	SPT_PROFILE_FUNCTION();
	
	outInputAssemblyStateInfo = VkPipelineInputAssemblyStateCreateInfo{ VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
    outInputAssemblyStateInfo.topology					= RHIToVulkan::GetPrimitiveTopology(pipelineBuildDef.pipelineDefinition.primitiveTopology);
    outInputAssemblyStateInfo.primitiveRestartEnable	= VK_FALSE;
}

static void BuildRasterizationStateInfo(const PipelineBuildDefinition& pipelineBuildDef, VkPipelineRasterizationStateCreateInfo& outRasterizationState)
{
	SPT_PROFILE_FUNCTION();

	const rhi::PipelineRasterizationDefinition& rasterizationDefinition = pipelineBuildDef.pipelineDefinition.rasterizationDefinition;

	outRasterizationState = VkPipelineRasterizationStateCreateInfo{ VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
    outRasterizationState.depthClampEnable			= VK_FALSE;
    outRasterizationState.rasterizerDiscardEnable	= VK_FALSE;
	outRasterizationState.polygonMode				= RHIToVulkan::GetPolygonMode(rasterizationDefinition.polygonMode);
	outRasterizationState.cullMode					= RHIToVulkan::GetCullMode(rasterizationDefinition.cullMode);
    outRasterizationState.frontFace					= VK_FRONT_FACE_CLOCKWISE;
    outRasterizationState.lineWidth					= 1.f;
}

static void BuildMultisampleStateInfo(const PipelineBuildDefinition& pipelineBuildDef, VkPipelineMultisampleStateCreateInfo& outMultisampleState)
{
	SPT_PROFILE_FUNCTION();

	const rhi::MultisamplingDefinition& multisampleStateDefinition = pipelineBuildDef.pipelineDefinition.multisamplingDefinition;

	outMultisampleState = VkPipelineMultisampleStateCreateInfo{ VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
    outMultisampleState.rasterizationSamples = RHIToVulkan::GetSampleCount(multisampleStateDefinition.samplesNum);
}

static void BuildDepthStencilStateInfo(const PipelineBuildDefinition& pipelineBuildDef, VkPipelineDepthStencilStateCreateInfo& outDepthStencilState)
{
	SPT_CHECK_NO_ENTRY();

	const rhi::PipelineRenderTargetsDefinition& renderTargetsDefinition = pipelineBuildDef.pipelineDefinition.renderTargetsDefinition;
	const rhi::DepthRenderTargetDefinition& depthRTDef = renderTargetsDefinition.depthRTDefinition;

	outDepthStencilState = VkPipelineDepthStencilStateCreateInfo{ VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
    outDepthStencilState.depthTestEnable = depthRTDef.depthCompareOp != rhi::EDepthCompareOperation::None;
	if (depthRTDef.enableDepthWrite)
	{
		outDepthStencilState.depthWriteEnable	= VK_TRUE;
		outDepthStencilState.depthCompareOp		= RHIToVulkan::GetCompareOp(depthRTDef.depthCompareOp);
	}
	else
	{
		outDepthStencilState.depthWriteEnable = VK_FALSE;
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

static void BuildColorBlendStateInfo(const PipelineBuildDefinition& pipelineBuildDef, lib::DynamicArray<VkPipelineColorBlendAttachmentState>& outBlendAttachmentStates, VkPipelineColorBlendStateCreateInfo& outColorBlendState)
{
	SPT_PROFILE_FUNCTION();

	outBlendAttachmentStates.clear();

	const rhi::PipelineRenderTargetsDefinition& renderTargetsDefinition = pipelineBuildDef.pipelineDefinition.renderTargetsDefinition;
	const lib::DynamicArray<rhi::ColorRenderTargetDefinition>& colorRTsDefinition = renderTargetsDefinition.colorRTsDefinition;

	std::transform(	std::cbegin(colorRTsDefinition), std::cend(colorRTsDefinition),
					std::back_inserter(outBlendAttachmentStates),
					[](const rhi::ColorRenderTargetDefinition& colorRTDefinition)
					{
						VkPipelineColorBlendAttachmentState attachmentBlendState{};
						attachmentBlendState.blendEnable = VK_TRUE;
						SetVulkanBlendType(colorRTDefinition.colorBlendType, attachmentBlendState.srcColorBlendFactor, attachmentBlendState.dstColorBlendFactor, attachmentBlendState.colorBlendOp);
						SetVulkanBlendType(colorRTDefinition.alphaBlendType, attachmentBlendState.srcAlphaBlendFactor, attachmentBlendState.dstAlphaBlendFactor, attachmentBlendState.alphaBlendOp);
						attachmentBlendState.colorWriteMask = RHIToVulkan::GetColorComponentFlags(colorRTDefinition.colorWriteMask);;

						return attachmentBlendState;
					});

	outColorBlendState = VkPipelineColorBlendStateCreateInfo{ VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
    outColorBlendState.logicOpEnable		= VK_FALSE;
    outColorBlendState.attachmentCount		= static_cast<Uint32>(outBlendAttachmentStates.size());
    outColorBlendState.pAttachments			= outBlendAttachmentStates.data();
	outColorBlendState.blendConstants[0]	= 0.f;
	outColorBlendState.blendConstants[1]	= 0.f;
	outColorBlendState.blendConstants[2]	= 0.f;
	outColorBlendState.blendConstants[3]	= 0.f;
}

static void BuildDynamicStatesInfo(const PipelineBuildDefinition& pipelineBuildDef, lib::DynamicArray<VkDynamicState>& outDynamicStates, VkPipelineDynamicStateCreateInfo& outDynamicStateInfo)
{
	SPT_PROFILE_FUNCTION();

	outDynamicStates.clear();

	outDynamicStates.emplace_back(VK_DYNAMIC_STATE_VIEWPORT);
	outDynamicStates.emplace_back(VK_DYNAMIC_STATE_SCISSOR);

	outDynamicStateInfo = VkPipelineDynamicStateCreateInfo{ VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
    outDynamicStateInfo.dynamicStateCount	= static_cast<Uint32>(outDynamicStates.size());
    outDynamicStateInfo.pDynamicStates		= outDynamicStates.data();
}

static void BuildPipelineRenderingInfo(const PipelineBuildDefinition& pipelineBuildDef, lib::DynamicArray<VkFormat>& outColorRTFormats, VkPipelineRenderingCreateInfo& outPipelineRenderingInfo)
{
	SPT_PROFILE_FUNCTION();

	outColorRTFormats.clear();

	const rhi::PipelineRenderTargetsDefinition& renderTargetsDefinition = pipelineBuildDef.pipelineDefinition.renderTargetsDefinition;
	const lib::DynamicArray<rhi::ColorRenderTargetDefinition>& colorRTsDefinition = renderTargetsDefinition.colorRTsDefinition;

	std::transform(	std::cbegin(colorRTsDefinition), std::cend(colorRTsDefinition),
					std::back_inserter(outColorRTFormats),
					[](const rhi::ColorRenderTargetDefinition& colorRTDefinition)
					{
						return RHIToVulkan::GetVulkanFormat(colorRTDefinition.format);
					});

	const VkFormat depthRTFormat	= RHIToVulkan::GetVulkanFormat(renderTargetsDefinition.depthRTDefinition.format);
	const VkFormat stencilRTFormat	= RHIToVulkan::GetVulkanFormat(renderTargetsDefinition.stencilRTDefinition.format);

	outPipelineRenderingInfo = VkPipelineRenderingCreateInfo{ VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
    outPipelineRenderingInfo.colorAttachmentCount		= static_cast<Uint32>(outColorRTFormats.size());
    outPipelineRenderingInfo.pColorAttachmentFormats	= outColorRTFormats.data();
    outPipelineRenderingInfo.depthAttachmentFormat		= depthRTFormat;
    outPipelineRenderingInfo.stencilAttachmentFormat	= stencilRTFormat;
}

static void BuildGraphicsPipelineInfo(const GraphicsPipelineBuildData& buildData, VkGraphicsPipelineCreateInfo& outPipelineInfo)
{
	SPT_PROFILE_FUNCTION();

	outPipelineInfo = VkGraphicsPipelineCreateInfo{ VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
    outPipelineInfo.flags				= 0;
    outPipelineInfo.stageCount			= static_cast<Uint32>(buildData.shaderStages.size());
    outPipelineInfo.pStages				= buildData.shaderStages.data();
    outPipelineInfo.pVertexInputState	= nullptr; // don't use standard vertex input
    outPipelineInfo.pInputAssemblyState	= &buildData.inputAssemblyStateInfo;
    outPipelineInfo.pTessellationState	= nullptr;
    outPipelineInfo.pViewportState		= nullptr; // we use dynamic viewports
    outPipelineInfo.pRasterizationState	= &buildData.rasterizationStateInfo;
    outPipelineInfo.pMultisampleState	= &buildData.multisampleStateInfo;
    outPipelineInfo.pDepthStencilState	= &buildData.depthStencilStateInfo;
    outPipelineInfo.pColorBlendState	= &buildData.colorBlendStateInfo;
    outPipelineInfo.pDynamicState		= &buildData.dynamicStateInfo;
    outPipelineInfo.layout				= buildData.pipelineLayout;
    outPipelineInfo.renderPass			= VK_NULL_HANDLE; // we use dynamic rendering, so render pass doesn't have to be specified
	outPipelineInfo.subpass				= 0;
    outPipelineInfo.basePipelineHandle	= VK_NULL_HANDLE;
    outPipelineInfo.basePipelineIndex	= 0;

	VulkanStructsLinkedList piplineInfoLinkedList(outPipelineInfo);
	piplineInfoLinkedList.Append(buildData.pipelineRenderingInfo);
}

} // gfx

namespace compute
{

/*
static void BuildComputePipelineInfo(const ComputePipelineBuildData& buildData, VkComputePipelineCreateInfo& outPipelineInfo)
{
	SPT_PROFILE_FUNCTION();

	outPipelineInfo = VkComputePipelineCreateInfo{ VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };
    //outPipelineInfo.stage				= &buildData.shaderStageInfo;
    outPipelineInfo.layout				= buildData.pipelineLayout;
    outPipelineInfo.basePipelineHandle	= VK_NULL_HANDLE;
    outPipelineInfo.basePipelineIndex	= 0;
}
*/

} // compute

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
	const Int32 buildIdx = m_currentBuildIdx.fetch_add(1, std::memory_order_acq_rel);

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

	return m_pipelineBuildDatas[buildIdx].pipelineID;
}

Bool PipelinesBuildsBatch::ShouldFlushPipelineBuilds(Int32 buildIdx) const
{
	return buildIdx == pipelinesBuildBatchMaxSize - 1;
}

void PipelinesBuildsBatch::GetPipelineCreateInfos(const VkGraphicsPipelineCreateInfo*& outPipelineInfos, Uint32& outPipelinesNum) const
{
	const Uint32 pipelinesNum = std::min(	static_cast<Uint32>(m_currentBuildIdx.load(std::memory_order_acquire)),
											static_cast<Uint32>(pipelinesBuildBatchMaxSize));
	
	outPipelineInfos = pipelinesNum > 0 ? m_pipelineCreateInfos.data() : nullptr;
	outPipelinesNum = pipelinesNum;
}

void PipelinesBuildsBatch::ResetPipelineBuildDatas()
{
	m_currentBuildIdx.store(0, std::memory_order_release);
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// PipelinesManager ==============================================================================

PipelinesManager::PipelinesManager()
	: m_piplineIDCounter(0)
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

	const PipelineID pipelineID = m_piplineIDCounter.fetch_add(1, std::memory_order_relaxed);

	Int32 buildIdx = idxNone<Int32>;

	{
		lib::SharedLockGuard pipelineBuildsLockGuard(m_pipelineBuildsLock);
		m_flushingPipelineBuilsCV.wait(pipelineBuildsLockGuard, [&, this]() { return TryBuildPipelineCreateData_AssumesLockedShared(pipelineBuildDef, pipelineID, buildIdx); });
	}

	SPT_CHECK(buildIdx != idxNone<Int32>);

	if (ShouldFlushPipelineBuilds(buildIdx))
	{
		BuildBatchedPipelines_AssumesLocked();
	}

	return pipelineID;
}

VkPipeline PipelinesManager::GetPipelineHandle(PipelineID pipelineID) const
{
	SPT_PROFILE_FUNCTION();

	if (pipelineID == idxNone<PipelineID>)
	{
		return VK_NULL_HANDLE;
	}

	SPT_CHECK(!m_currentBuildsBatch.HasPendingBuilds());

	const auto pipelineHandle = m_cachedPipelines.find(pipelineID);
	return pipelineHandle != std::cend(m_cachedPipelines) ? pipelineHandle->second : VK_NULL_HANDLE;
}

Bool PipelinesManager::TryBuildPipelineCreateData_AssumesLockedShared(const PipelineBuildDefinition& pipelineBuildDef, PipelineID pipelineID, Int32& outBuildIdx)
{
	SPT_PROFILE_FUNCTION();

	outBuildIdx = AcquireNewBuildIdx_AssumesLockedShared();
	const Bool success = (outBuildIdx != idxNone<Int32>);

	if (success)
	{
		BuildPipelineCreateData(pipelineBuildDef, pipelineID, outBuildIdx);
	}

	return success;
}

Int32 PipelinesManager::AcquireNewBuildIdx_AssumesLockedShared()
{
	// this must be called when locked, so that it won't increment pipelines num before build
	const Int32 buildIdx = m_currentBuildsBatch.AcquireNewBuildIdx();
	return buildIdx;
}

void PipelinesManager::BuildPipelineCreateData(const PipelineBuildDefinition& pipelineBuildDef, PipelineID pipelineID, Int32 buildIdx)
{
	SPT_PROFILE_FUNCTION();

	PipelinesBuildsBatch::BatchedPipelineBuildRef batchedBuildRef	= m_currentBuildsBatch.GetPiplineCreateData(buildIdx);
	GraphicsPipelineBuildData& buildData									= std::get<GraphicsPipelineBuildData&>(batchedBuildRef);
	VkGraphicsPipelineCreateInfo& pipelineInfo						= std::get<VkGraphicsPipelineCreateInfo&>(batchedBuildRef);

	buildData.pipelineID = pipelineID;
	helpers::gfx::BuildShaderInfos(pipelineBuildDef, buildData.shaderStages);
	helpers::gfx::BuildInputAssemblyInfo(pipelineBuildDef, buildData.inputAssemblyStateInfo);
	helpers::gfx::BuildRasterizationStateInfo(pipelineBuildDef, buildData.rasterizationStateInfo);
	helpers::gfx::BuildMultisampleStateInfo(pipelineBuildDef, buildData.multisampleStateInfo);
	helpers::gfx::BuildDepthStencilStateInfo(pipelineBuildDef, buildData.depthStencilStateInfo);
	helpers::gfx::BuildColorBlendStateInfo(pipelineBuildDef, buildData.blendAttachmentStates, buildData.colorBlendStateInfo);
	helpers::gfx::BuildDynamicStatesInfo(pipelineBuildDef, buildData.dynamicStates, buildData.dynamicStateInfo);
	helpers::gfx::BuildPipelineRenderingInfo(pipelineBuildDef, buildData.colorRTFormats, buildData.pipelineRenderingInfo);
	buildData.pipelineLayout = pipelineBuildDef.layout.GetHandle();

	helpers::gfx::BuildGraphicsPipelineInfo(buildData, pipelineInfo);
}

Bool PipelinesManager::ShouldFlushPipelineBuilds(Int32 buildIdx) const
{
	return m_currentBuildsBatch.ShouldFlushPipelineBuilds(buildIdx);
}

void PipelinesManager::BuildBatchedPipelines_AssumesLocked()
{
	SPT_PROFILE_FUNCTION();

	const lib::UniqueLockGuard pipelineBuildsLockGuard(m_pipelineBuildsLock);

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
			const PipelineID pipelineID = m_currentBuildsBatch.GetPipelineID(static_cast<Int32>(idx));
			const VkPipeline pipelineHandle = pipelines[idx];

			m_cachedPipelines.emplace(pipelineID, pipelineHandle);
		}

		m_currentBuildsBatch.ResetPipelineBuildDatas();
	}

	m_flushingPipelineBuilsCV.notify_all();
}

} // spt::vulkan
