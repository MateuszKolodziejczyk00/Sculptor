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

static SPT_INLINE void ReleasePipelineResource(VkPipeline pipelineHandle)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(pipelineHandle != VK_NULL_HANDLE);

	vkDestroyPipeline(VulkanRHI::GetDeviceHandle(), pipelineHandle, VulkanRHI::GetAllocationCallbacks());
}

namespace gfx
{

static void BuildShaderInfos(const GraphicsPipelineBuildDefinition& pipelineBuildDef, lib::DynamicArray<VkPipelineShaderStageCreateInfo>& outShaderStageInfos)
{
	SPT_PROFILER_FUNCTION();

	outShaderStageInfos.clear();

	const lib::DynamicArray<RHIShaderModule>& shaderModules = pipelineBuildDef.shaderStagesDef.shaderModules;

	std::transform(	std::cbegin(shaderModules), std::cend(shaderModules),
					std::back_inserter(outShaderStageInfos),
					&BuildPipelineShaderStageInfo);
}

static void BuildInputAssemblyInfo(const GraphicsPipelineBuildDefinition& pipelineBuildDef, VkPipelineInputAssemblyStateCreateInfo& outInputAssemblyStateInfo)
{
	SPT_PROFILER_FUNCTION();
	
	outInputAssemblyStateInfo = VkPipelineInputAssemblyStateCreateInfo{ VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
    outInputAssemblyStateInfo.topology					= RHIToVulkan::GetPrimitiveTopology(pipelineBuildDef.pipelineDefinition.primitiveTopology);
    outInputAssemblyStateInfo.primitiveRestartEnable	= VK_FALSE;
}

static void BuildRasterizationStateInfo(const GraphicsPipelineBuildDefinition& pipelineBuildDef, VkPipelineRasterizationStateCreateInfo& outRasterizationState)
{
	SPT_PROFILER_FUNCTION();

	const rhi::PipelineRasterizationDefinition& rasterizationDefinition = pipelineBuildDef.pipelineDefinition.rasterizationDefinition;

	outRasterizationState = VkPipelineRasterizationStateCreateInfo{ VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
    outRasterizationState.depthClampEnable			= VK_FALSE;
    outRasterizationState.rasterizerDiscardEnable	= VK_FALSE;
	outRasterizationState.polygonMode				= RHIToVulkan::GetPolygonMode(rasterizationDefinition.polygonMode);
	outRasterizationState.cullMode					= RHIToVulkan::GetCullMode(rasterizationDefinition.cullMode);
    outRasterizationState.frontFace					= VK_FRONT_FACE_CLOCKWISE;
    outRasterizationState.lineWidth					= 1.f;
}

static void BuildMultisampleStateInfo(const GraphicsPipelineBuildDefinition& pipelineBuildDef, VkPipelineMultisampleStateCreateInfo& outMultisampleState)
{
	SPT_PROFILER_FUNCTION();

	const rhi::MultisamplingDefinition& multisampleStateDefinition = pipelineBuildDef.pipelineDefinition.multisamplingDefinition;

	outMultisampleState = VkPipelineMultisampleStateCreateInfo{ VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
    outMultisampleState.rasterizationSamples = RHIToVulkan::GetSampleCount(multisampleStateDefinition.samplesNum);
}

static void BuildDepthStencilStateInfo(const GraphicsPipelineBuildDefinition& pipelineBuildDef, VkPipelineDepthStencilStateCreateInfo& outDepthStencilState)
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

static void BuildColorBlendStateInfo(const GraphicsPipelineBuildDefinition& pipelineBuildDef, lib::DynamicArray<VkPipelineColorBlendAttachmentState>& outBlendAttachmentStates, VkPipelineColorBlendStateCreateInfo& outColorBlendState)
{
	SPT_PROFILER_FUNCTION();

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

static void BuildDynamicStatesInfo(const GraphicsPipelineBuildDefinition& pipelineBuildDef, lib::DynamicArray<VkDynamicState>& outDynamicStates, VkPipelineDynamicStateCreateInfo& outDynamicStateInfo)
{
	SPT_PROFILER_FUNCTION();

	outDynamicStates.clear();

	outDynamicStates.emplace_back(VK_DYNAMIC_STATE_VIEWPORT);
	outDynamicStates.emplace_back(VK_DYNAMIC_STATE_SCISSOR);

	outDynamicStateInfo = VkPipelineDynamicStateCreateInfo{ VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
    outDynamicStateInfo.dynamicStateCount	= static_cast<Uint32>(outDynamicStates.size());
    outDynamicStateInfo.pDynamicStates		= outDynamicStates.data();
}

static void BuildPipelineRenderingInfo(const GraphicsPipelineBuildDefinition& pipelineBuildDef, lib::DynamicArray<VkFormat>& outColorRTFormats, VkPipelineRenderingCreateInfo& outPipelineRenderingInfo)
{
	SPT_PROFILER_FUNCTION();

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
	SPT_PROFILER_FUNCTION();

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

static void BuildGraphicsPipelineCreateData(const GraphicsPipelineBuildDefinition& pipelineBuildDef, PipelineID pipelineID, GraphicsPipelineBuildData& outBuildData, VkGraphicsPipelineCreateInfo& outPipelineInfo)
{
	SPT_PROFILER_FUNCTION();

	outBuildData.pipelineID = pipelineID;
	outBuildData.pipelineLayout = pipelineBuildDef.layout.GetHandle();
	BuildShaderInfos(pipelineBuildDef, outBuildData.shaderStages);
	BuildInputAssemblyInfo(pipelineBuildDef, outBuildData.inputAssemblyStateInfo);
	BuildRasterizationStateInfo(pipelineBuildDef, outBuildData.rasterizationStateInfo);
	BuildMultisampleStateInfo(pipelineBuildDef, outBuildData.multisampleStateInfo);
	BuildDepthStencilStateInfo(pipelineBuildDef, outBuildData.depthStencilStateInfo);
	BuildColorBlendStateInfo(pipelineBuildDef, outBuildData.blendAttachmentStates, outBuildData.colorBlendStateInfo);
	BuildDynamicStatesInfo(pipelineBuildDef, outBuildData.dynamicStates, outBuildData.dynamicStateInfo);
	BuildPipelineRenderingInfo(pipelineBuildDef, outBuildData.colorRTFormats, outBuildData.pipelineRenderingInfo);

	BuildGraphicsPipelineInfo(outBuildData, outPipelineInfo);
}

// out pipelines are guaranteed to be big-enough
static void BuildGraphicsPipelines(const VkGraphicsPipelineCreateInfo* pipelineInfos, Uint32 pipelinesNum, VkPipeline* outPipelines)
{
	SPT_PROFILER_FUNCTION();

	const VkPipelineCache pipelineCache = VK_NULL_HANDLE;

	SPT_VK_CHECK(vkCreateGraphicsPipelines(	VulkanRHI::GetDeviceHandle(),
											pipelineCache,
											pipelinesNum,
											pipelineInfos,
											VulkanRHI::GetAllocationCallbacks(),
											outPipelines));
}

} // gfx

namespace compute
{

static void BuildComputePipelineInfo(const ComputePipelineBuildDefinition& computePipelineDef, const ComputePipelineBuildData& buildData, VkComputePipelineCreateInfo& outPipelineInfo)
{
	SPT_PROFILER_FUNCTION();

	outPipelineInfo = VkComputePipelineCreateInfo{ VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };
	outPipelineInfo.stage				= BuildPipelineShaderStageInfo(computePipelineDef.computeShaderModule);
    outPipelineInfo.layout				= buildData.pipelineLayout;
    outPipelineInfo.basePipelineHandle	= VK_NULL_HANDLE;
    outPipelineInfo.basePipelineIndex	= 0;
}

static void BuildComputePipelineCreateData(const ComputePipelineBuildDefinition& pipelineBuildDef, PipelineID pipelineID, ComputePipelineBuildData& outBuildData, VkComputePipelineCreateInfo& outPipelineInfo)
{
	SPT_PROFILER_FUNCTION();

	outBuildData.pipelineID = pipelineID;
	outBuildData.pipelineLayout = pipelineBuildDef.layout.GetHandle();

	BuildComputePipelineInfo(pipelineBuildDef, outBuildData, outPipelineInfo);
}

// out pipelines are guaranteed to be big-enough
static void BuildComputePipelines(const VkComputePipelineCreateInfo* pipelineInfos, Uint32 pipelinesNum, VkPipeline* outPipelines)
{
	SPT_PROFILER_FUNCTION();

	const VkPipelineCache pipelineCache = VK_NULL_HANDLE;

	SPT_VK_CHECK(vkCreateComputePipelines(	VulkanRHI::GetDeviceHandle(),
											pipelineCache,
											pipelinesNum,
											pipelineInfos,
											VulkanRHI::GetAllocationCallbacks(),
											outPipelines));
}

} // compute

} // helpers

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
	SPT_PROFILER_FUNCTION();

	std::for_each(std::cbegin(m_cachedPipelines), std::cend(m_cachedPipelines),
		[](const auto& idToPipeline)
		{
			const VkPipeline pipeline = idToPipeline.second;
			SPT_CHECK(pipeline != VK_NULL_HANDLE);
			helpers::ReleasePipelineResource(pipeline);
		});

	m_cachedPipelines.clear();
}

PipelineID PipelinesManager::BuildGraphicsPipelineDeferred(const GraphicsPipelineBuildDefinition& pipelineBuildDef)
{
	SPT_PROFILER_FUNCTION();

	const PipelineID pipelineID = m_piplineIDCounter.fetch_add(1, std::memory_order_relaxed);

	Bool shouldFlushPipelineBuilds = false;

	m_graphicsPipelinesBatchBuilder.AppendPipelineCreateData(
		[&pipelineBuildDef, pipelineID](GraphicsPipelineBuildData& outBuildData, VkGraphicsPipelineCreateInfo& outPipelineInfo)
		{
			helpers::gfx::BuildGraphicsPipelineCreateData(pipelineBuildDef, pipelineID, outBuildData, outPipelineInfo);
		},
		shouldFlushPipelineBuilds);

	if (shouldFlushPipelineBuilds)
	{
		FlushPendingGraphicsPipelines();
	}

	return pipelineID;
}

void PipelinesManager::FlushPendingGraphicsPipelines()
{
	SPT_PROFILER_FUNCTION();

	const lib::DynamicArray<std::pair<PipelineID, VkPipeline>> createdPipelines = m_graphicsPipelinesBatchBuilder.BuildPendingPipelines(&helpers::gfx::BuildGraphicsPipelines);
	CacheCreatedPipelines(createdPipelines);
}

PipelineID PipelinesManager::BuildComputePipelineDeferred(const ComputePipelineBuildDefinition& pipelineBuildDef)
{
	SPT_PROFILER_FUNCTION();

	const PipelineID pipelineID = m_piplineIDCounter.fetch_add(1, std::memory_order_relaxed);

	Bool shouldFlushPipelineBuilds = false;

	m_computePipelinesBatchBuilder.AppendPipelineCreateData(
		[&pipelineBuildDef, pipelineID](ComputePipelineBuildData& outBuildData, VkComputePipelineCreateInfo& outPipelineInfo)
		{
			helpers::compute::BuildComputePipelineCreateData(pipelineBuildDef, pipelineID, outBuildData, outPipelineInfo);
		},
		shouldFlushPipelineBuilds);

	if (shouldFlushPipelineBuilds)
	{
		FlushPendingComputePipelines();
	}

	return pipelineID;
}

void PipelinesManager::FlushPendingComputePipelines()
{
	SPT_PROFILER_FUNCTION();

	const lib::DynamicArray<std::pair<PipelineID, VkPipeline>> createdPipelines = m_computePipelinesBatchBuilder.BuildPendingPipelines(&helpers::compute::BuildComputePipelines);
	CacheCreatedPipelines(createdPipelines);
}

void PipelinesManager::ReleasePipeline(PipelineID pipelineID)
{
	SPT_PROFILER_FUNCTION();

	const auto foundPipeline = m_cachedPipelines.find(pipelineID);
	if (foundPipeline != std::cend(m_cachedPipelines))
	{
		helpers::ReleasePipelineResource(foundPipeline->second);
	}
}

VkPipeline PipelinesManager::GetPipelineHandle(PipelineID pipelineID) const
{
	SPT_PROFILER_FUNCTION();

	if (pipelineID == idxNone<PipelineID>)
	{
		return VK_NULL_HANDLE;
	}

	// not sure if we should flush pending build on get, maybe flush should be earlies, so that all pipelines are built before commands are processed
	// that's why have this check for now
	SPT_CHECK(!m_graphicsPipelinesBatchBuilder.HasPendingBuilds() && !m_computePipelinesBatchBuilder.HasPendingBuilds());

	const auto pipelineHandle = m_cachedPipelines.find(pipelineID);
	return pipelineHandle != std::cend(m_cachedPipelines) ? pipelineHandle->second : VK_NULL_HANDLE;
}

void PipelinesManager::CacheCreatedPipelines(const lib::DynamicArray<std::pair<PipelineID, VkPipeline>>& createdPipelines)
{
	SPT_PROFILER_FUNCTION();

	std::copy(std::cbegin(createdPipelines), std::cend(createdPipelines), std::inserter(m_cachedPipelines, std::end(m_cachedPipelines)));
}

} // spt::vulkan
