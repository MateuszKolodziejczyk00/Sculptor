#include "RHIPipeline.h"
#include "Vulkan/VulkanRHI.h"
#include "Vulkan/Pipeline/PipelineLayoutsManager.h"
#include "Vulkan/VulkanRHIUtils.h"
#include "Vulkan/VulkanUtils.h"

namespace spt::vulkan
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// Helpers =======================================================================================

namespace helpers
{

static VkPipelineShaderStageCreateInfo BuildPipelineShaderStageInfo(const rhi::RHIShaderModule& shaderModule)
{
	SPT_CHECK(shaderModule.IsValid());

	VkPipelineShaderStageCreateInfo stageInfo{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
	stageInfo.stage = RHIToVulkan::GetShaderStage(shaderModule.GetStage());
	stageInfo.module = shaderModule.GetHandle();
	stageInfo.pName = shaderModule.GetEntryPoint().GetData();

	return stageInfo;
}

static void ReleasePipelineResource(VkPipeline pipelineHandle)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(pipelineHandle != VK_NULL_HANDLE);

	vkDestroyPipeline(VulkanRHI::GetDeviceHandle(), pipelineHandle, VulkanRHI::GetAllocationCallbacks());
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// Gfx ===========================================================================================

namespace gfx
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


static lib::DynamicArray<VkPipelineShaderStageCreateInfo> BuildShaderInfos(const GraphicsPipelineBuildDefinition& pipelineBuildDef)
{
	SPT_PROFILER_FUNCTION();

	const lib::DynamicArray<RHIShaderModule>& shaderModules = pipelineBuildDef.shaderStagesDef.shaderModules;

	lib::DynamicArray<VkPipelineShaderStageCreateInfo> outShaderStageInfos;
	outShaderStageInfos.reserve(shaderModules.size());

	std::transform(std::cbegin(shaderModules), std::cend(shaderModules),
				   std::back_inserter(outShaderStageInfos),
				   &BuildPipelineShaderStageInfo);

	return outShaderStageInfos;
}

static VkPipelineInputAssemblyStateCreateInfo BuildInputAssemblyInfo(const GraphicsPipelineBuildDefinition& pipelineBuildDef)
{
	SPT_PROFILER_FUNCTION();

	VkPipelineInputAssemblyStateCreateInfo assemblyState{ VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
    assemblyState.topology					= RHIToVulkan::GetPrimitiveTopology(pipelineBuildDef.pipelineDefinition.primitiveTopology);
    assemblyState.primitiveRestartEnable	= VK_FALSE;

	return assemblyState;
}

static VkPipelineRasterizationStateCreateInfo BuildRasterizationStateInfo(const GraphicsPipelineBuildDefinition& pipelineBuildDef, VkPipelineRasterizationConservativeStateCreateInfoEXT& outConservativeRasterizationState)
{
	SPT_PROFILER_FUNCTION();

	const rhi::PipelineRasterizationDefinition& rasterizationDefinition = pipelineBuildDef.pipelineDefinition.rasterizationDefinition;

	VkPipelineRasterizationStateCreateInfo rasterizationState{ VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
    rasterizationState.depthClampEnable			= VK_FALSE;
	rasterizationState.polygonMode				= RHIToVulkan::GetPolygonMode(rasterizationDefinition.polygonMode);
	rasterizationState.cullMode					= RHIToVulkan::GetCullMode(rasterizationDefinition.cullMode);
    rasterizationState.frontFace				= VK_FRONT_FACE_CLOCKWISE;
    rasterizationState.lineWidth				= 1.f;

	if (rasterizationDefinition.rasterizationType != rhi::ERasterizationType::Default)
	{
		const VkConservativeRasterizationModeEXT mode = rasterizationDefinition.rasterizationType == rhi::ERasterizationType::ConservativeOverestimate
													  ? VK_CONSERVATIVE_RASTERIZATION_MODE_OVERESTIMATE_EXT
													  : VK_CONSERVATIVE_RASTERIZATION_MODE_UNDERESTIMATE_EXT;
		outConservativeRasterizationState = VkPipelineRasterizationConservativeStateCreateInfoEXT{ VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_CONSERVATIVE_STATE_CREATE_INFO_EXT };
		outConservativeRasterizationState.conservativeRasterizationMode = mode;

		rasterizationState.pNext = &outConservativeRasterizationState;
	}

	return rasterizationState;
}

static VkPipelineMultisampleStateCreateInfo BuildMultisampleStateInfo(const GraphicsPipelineBuildDefinition& pipelineBuildDef)
{
	SPT_PROFILER_FUNCTION();

	const rhi::MultisamplingDefinition& multisampleStateDefinition = pipelineBuildDef.pipelineDefinition.multisamplingDefinition;

	VkPipelineMultisampleStateCreateInfo multisampleState{ VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
	multisampleState.rasterizationSamples = RHIToVulkan::GetSampleCount(multisampleStateDefinition.samplesNum);

	return multisampleState;
}

static VkPipelineDepthStencilStateCreateInfo BuildDepthStencilStateInfo(const GraphicsPipelineBuildDefinition& pipelineBuildDef)
{
	SPT_PROFILER_FUNCTION();

	const rhi::PipelineRenderTargetsDefinition& renderTargetsDefinition = pipelineBuildDef.pipelineDefinition.renderTargetsDefinition;
	const rhi::DepthRenderTargetDefinition& depthRTDef = renderTargetsDefinition.depthRTDefinition;

	const Bool enableCompare = depthRTDef.depthCompareOp != rhi::ECompareOp::None;

	VkPipelineDepthStencilStateCreateInfo depthStencilState{ VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
    depthStencilState.depthTestEnable = enableCompare;
	if (enableCompare)
	{
		depthStencilState.depthCompareOp = RHIToVulkan::GetCompareOp(depthRTDef.depthCompareOp);
	}
	depthStencilState.depthWriteEnable = depthRTDef.enableDepthWrite;

	return depthStencilState;
}

static VkPipelineVertexInputStateCreateInfo BuildVertexInputState(const GraphicsPipelineBuildDefinition& pipelineBuildDef)
{
	VkPipelineVertexInputStateCreateInfo vertexInputState{ VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };

	// We're not using any vertex input
	return vertexInputState;
}

static VkPipelineViewportStateCreateInfo BuildViewportState(const GraphicsPipelineBuildDefinition& pipelineBuildDef)
{
	VkPipelineViewportStateCreateInfo viewportStateCreateInfo{ VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
	viewportStateCreateInfo.viewportCount	= 1;
	viewportStateCreateInfo.scissorCount	= 1;

	return viewportStateCreateInfo;
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

static VkPipelineColorBlendStateCreateInfo BuildColorBlendStateInfo(const GraphicsPipelineBuildDefinition& pipelineBuildDef, lib::DynamicArray<VkPipelineColorBlendAttachmentState>& outBlendAttachmentStates)
{
	SPT_PROFILER_FUNCTION();

	const rhi::PipelineRenderTargetsDefinition& renderTargetsDefinition = pipelineBuildDef.pipelineDefinition.renderTargetsDefinition;
	const lib::DynamicArray<rhi::ColorRenderTargetDefinition>& colorRTsDefinition = renderTargetsDefinition.colorRTsDefinition;

	outBlendAttachmentStates.reserve(colorRTsDefinition.size());

	std::transform(std::cbegin(colorRTsDefinition), std::cend(colorRTsDefinition),
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

	VkPipelineColorBlendStateCreateInfo colorBlendState{ VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
    colorBlendState.logicOpEnable		= VK_FALSE;
    colorBlendState.attachmentCount		= static_cast<Uint32>(outBlendAttachmentStates.size());
    colorBlendState.pAttachments		= outBlendAttachmentStates.data();
	colorBlendState.blendConstants[0]	= 0.f;
	colorBlendState.blendConstants[1]	= 0.f;
	colorBlendState.blendConstants[2]	= 0.f;
	colorBlendState.blendConstants[3]	= 0.f;

	return colorBlendState;
}

static VkPipelineDynamicStateCreateInfo BuildDynamicStatesInfo(const GraphicsPipelineBuildDefinition& pipelineBuildDef, lib::DynamicArray<VkDynamicState>& outDynamicStates)
{
	SPT_PROFILER_FUNCTION();

	outDynamicStates.emplace_back(VK_DYNAMIC_STATE_VIEWPORT);
	outDynamicStates.emplace_back(VK_DYNAMIC_STATE_SCISSOR);

	VkPipelineDynamicStateCreateInfo dynamicStateInfo{ VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
    dynamicStateInfo.dynamicStateCount	= static_cast<Uint32>(outDynamicStates.size());
    dynamicStateInfo.pDynamicStates		= outDynamicStates.data();

	return dynamicStateInfo;
}

static VkPipelineRenderingCreateInfo BuildPipelineRenderingInfo(const GraphicsPipelineBuildDefinition& pipelineBuildDef, lib::DynamicArray<VkFormat>& outColorRTFormats)
{
	SPT_PROFILER_FUNCTION();

	const rhi::PipelineRenderTargetsDefinition& renderTargetsDefinition = pipelineBuildDef.pipelineDefinition.renderTargetsDefinition;
	const lib::DynamicArray<rhi::ColorRenderTargetDefinition>& colorRTsDefinition = renderTargetsDefinition.colorRTsDefinition;

	outColorRTFormats.reserve(colorRTsDefinition.size());

	std::transform(std::cbegin(colorRTsDefinition), std::cend(colorRTsDefinition),
				   std::back_inserter(outColorRTFormats),
				   [](const rhi::ColorRenderTargetDefinition& colorRTDefinition)
				   {
					   return RHIToVulkan::GetVulkanFormat(colorRTDefinition.format);
				   });

	const VkFormat depthRTFormat	= RHIToVulkan::GetVulkanFormat(renderTargetsDefinition.depthRTDefinition.format);
	const VkFormat stencilRTFormat	= RHIToVulkan::GetVulkanFormat(renderTargetsDefinition.stencilRTDefinition.format);

	VkPipelineRenderingCreateInfo pipelineRenderingInfo{ VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
    pipelineRenderingInfo.colorAttachmentCount		= static_cast<Uint32>(outColorRTFormats.size());
    pipelineRenderingInfo.pColorAttachmentFormats	= outColorRTFormats.data();
    pipelineRenderingInfo.depthAttachmentFormat		= depthRTFormat;
    pipelineRenderingInfo.stencilAttachmentFormat	= stencilRTFormat;

	return pipelineRenderingInfo;
}

static VkPipeline BuildGraphicsPipeline(const GraphicsPipelineBuildDefinition& pipelineBuildDef)
{
	SPT_PROFILER_FUNCTION();

	VkPipelineRasterizationConservativeStateCreateInfoEXT conservativeRasterizationState{};

	const lib::DynamicArray<VkPipelineShaderStageCreateInfo> shaderStages	= BuildShaderInfos(pipelineBuildDef);
	const VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateInfo		= BuildInputAssemblyInfo(pipelineBuildDef);
	const VkPipelineRasterizationStateCreateInfo rasterizationStateInfo		= BuildRasterizationStateInfo(pipelineBuildDef, OUT conservativeRasterizationState);
	const VkPipelineMultisampleStateCreateInfo multisampleStateInfo			= BuildMultisampleStateInfo(pipelineBuildDef);
	const VkPipelineDepthStencilStateCreateInfo depthStencilStateInfo		= BuildDepthStencilStateInfo(pipelineBuildDef);
	const VkPipelineVertexInputStateCreateInfo vertexInputState				= BuildVertexInputState(pipelineBuildDef);
    const VkPipelineViewportStateCreateInfo viewportStateInfo				= BuildViewportState(pipelineBuildDef);

	lib::DynamicArray<VkPipelineColorBlendAttachmentState> blendAttachmentStates;
	const VkPipelineColorBlendStateCreateInfo colorBlendStateInfo = BuildColorBlendStateInfo(pipelineBuildDef, OUT blendAttachmentStates);

	lib::DynamicArray<VkDynamicState> dynamicStates;
	const VkPipelineDynamicStateCreateInfo dynamicStateInfo = BuildDynamicStatesInfo(pipelineBuildDef, OUT dynamicStates);

	lib::DynamicArray<VkFormat> colorRTFormats;
	const VkPipelineRenderingCreateInfo pipelineRenderingInfo = BuildPipelineRenderingInfo(pipelineBuildDef, OUT colorRTFormats);

	VkGraphicsPipelineCreateInfo pipelineInfo{ VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
    pipelineInfo.flags					= 0;
    pipelineInfo.stageCount				= static_cast<Uint32>(shaderStages.size());
    pipelineInfo.pStages				= shaderStages.data();
    pipelineInfo.pVertexInputState		= &vertexInputState;
    pipelineInfo.pInputAssemblyState	= &inputAssemblyStateInfo;
    pipelineInfo.pTessellationState		= VK_NULL_HANDLE;
    pipelineInfo.pViewportState			= &viewportStateInfo;
    pipelineInfo.pRasterizationState	= &rasterizationStateInfo;
    pipelineInfo.pMultisampleState		= &multisampleStateInfo;
    pipelineInfo.pDepthStencilState		= &depthStencilStateInfo;
    pipelineInfo.pColorBlendState		= &colorBlendStateInfo;
    pipelineInfo.pDynamicState			= &dynamicStateInfo;
    pipelineInfo.layout					= pipelineBuildDef.layout.GetHandle();
    pipelineInfo.renderPass				= VK_NULL_HANDLE; // we use dynamic rendering, so render pass doesn't have to be specified
	pipelineInfo.subpass				= 0;
    pipelineInfo.basePipelineHandle		= VK_NULL_HANDLE;
    pipelineInfo.basePipelineIndex		= 0;

	VulkanStructsLinkedList piplineInfoLinkedList(pipelineInfo);
	piplineInfoLinkedList.Append(pipelineRenderingInfo);

	VkPipeline pipelineHandle = VK_NULL_HANDLE;

	const VkPipelineCache pipelineCache = VK_NULL_HANDLE;

	SPT_VK_CHECK(vkCreateGraphicsPipelines(VulkanRHI::GetDeviceHandle(),
										   pipelineCache,
										   1,
										   &pipelineInfo,
										   VulkanRHI::GetAllocationCallbacks(),
										   &pipelineHandle));

	return pipelineHandle;
}

} // gfx

//////////////////////////////////////////////////////////////////////////////////////////////////
// Compute =======================================================================================
 
namespace compute
{

struct ComputePipelineBuildDefinition
{
	ComputePipelineBuildDefinition(const RHIShaderModule& inComputeShaderModule, const PipelineLayout& inPipelineLayout)
		: computeShaderModule(inComputeShaderModule)
		, layout(inPipelineLayout)
	{ }

	const RHIShaderModule&		computeShaderModule;
	const PipelineLayout&		layout;
};

static VkPipeline BuildComputePipeline(const ComputePipelineBuildDefinition& pipelineBuildDef)
{
	SPT_PROFILER_FUNCTION();

	VkComputePipelineCreateInfo pipelineInfo{ VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };
	pipelineInfo.stage				= BuildPipelineShaderStageInfo(pipelineBuildDef.computeShaderModule);
	pipelineInfo.layout				= pipelineBuildDef.layout.GetHandle();
    pipelineInfo.basePipelineHandle	= VK_NULL_HANDLE;
    pipelineInfo.basePipelineIndex	= 0;

	const VkPipelineCache pipelineCache = VK_NULL_HANDLE;

	VkPipeline pipelineHandle = VK_NULL_HANDLE;

	SPT_VK_CHECK(vkCreateComputePipelines(VulkanRHI::GetDeviceHandle(),
										  pipelineCache,
										  1,
										  &pipelineInfo,
										  VulkanRHI::GetAllocationCallbacks(),
										  &pipelineHandle));

	return pipelineHandle;
}

} // compute

//////////////////////////////////////////////////////////////////////////////////////////////////
// Ray Tracing ===================================================================================
 
namespace ray_tracing
{

struct RayTracingPipelineBuildDefinition
{
	RayTracingPipelineBuildDefinition(const rhi::RayTracingShadersDefinition& inShadersDef, const rhi::RayTracingPipelineDefinition& inPipelineDefintion, const PipelineLayout& inPipelineLayout)
		: shadersDef(inShadersDef)
		, pipelineDefinition(inPipelineDefintion)
		, layout(inPipelineLayout)
	{ }

	const rhi::RayTracingShadersDefinition&		shadersDef;
	const rhi::RayTracingPipelineDefinition&	pipelineDefinition;
	const PipelineLayout&						layout;
};

static void AppendShaderStageInfos(INOUT lib::DynamicArray<VkPipelineShaderStageCreateInfo>& stageInfos, const lib::DynamicArray<RHIShaderModule>& shaderModules)
{
	for (const RHIShaderModule& module : shaderModules)
	{
		stageInfos.emplace_back(helpers::BuildPipelineShaderStageInfo(module));
	}
}

static VkRayTracingShaderGroupCreateInfoKHR CreateGeneralShaderGroup(Uint32 generalShaderIdx)
{
	VkRayTracingShaderGroupCreateInfoKHR group{ VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR };
	group.type					= VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
	group.generalShader			= generalShaderIdx;
	group.closestHitShader		= VK_SHADER_UNUSED_KHR;
	group.anyHitShader			= VK_SHADER_UNUSED_KHR;
	group.intersectionShader	= VK_SHADER_UNUSED_KHR;

	return group;
}

static VkPipeline BuildRayTracingPipeline(const RayTracingPipelineBuildDefinition& pipelineBuildDef)
{
	SPT_PROFILER_FUNCTION();

	const rhi::RayTracingPipelineDefinition& pipelineDef	= pipelineBuildDef.pipelineDefinition;
	const rhi::RayTracingShadersDefinition& shadersDef		= pipelineBuildDef.shadersDef;

	SPT_CHECK(shadersDef.rayGenerationModule.IsValid());
	
	// We need to add 1 because of the ray generation shader
	const SizeType shadersNum = shadersDef.closestHitModules.size() + shadersDef.missModules.size() + 1;

	lib::DynamicArray<VkPipelineShaderStageCreateInfo> shaderStages;
	shaderStages.reserve(shadersNum);
	shaderStages.emplace_back(helpers::BuildPipelineShaderStageInfo(shadersDef.rayGenerationModule));
	AppendShaderStageInfos(shaderStages, shadersDef.closestHitModules);
	AppendShaderStageInfos(shaderStages, shadersDef.missModules);

	Uint32 shaderIdx = 0;

	lib::DynamicArray<VkRayTracingShaderGroupCreateInfoKHR>	shaderGroups;
	shaderGroups.reserve(shadersNum);

	// Ray generation shader group
	shaderGroups.emplace_back(CreateGeneralShaderGroup(shaderIdx++));

	// Closest hit shader groups
	std::generate_n(std::back_inserter(shaderGroups), shadersDef.closestHitModules.size(),
					[&shaderIdx]
					{
						VkRayTracingShaderGroupCreateInfoKHR group{ VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR };
						group.type					= VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
						group.generalShader			= VK_SHADER_UNUSED_KHR;
						group.closestHitShader		= shaderIdx++;
						group.anyHitShader			= VK_SHADER_UNUSED_KHR;
						group.intersectionShader	= VK_SHADER_UNUSED_KHR;
	
						return group;
					});

	// Miss shader groups
	std::generate_n(std::back_inserter(shaderGroups), shadersDef.closestHitModules.size(), [ &shaderIdx ] { return CreateGeneralShaderGroup(shaderIdx++); });

	VkRayTracingPipelineCreateInfoKHR pipelineInfo{ VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR };
	pipelineInfo.stageCount						= static_cast<Uint32>(shaderStages.size());
	pipelineInfo.pStages						= shaderStages.data();
	pipelineInfo.groupCount						= static_cast<Uint32>(shaderGroups.size());
	pipelineInfo.pGroups						= shaderGroups.data();
	pipelineInfo.maxPipelineRayRecursionDepth	= pipelineDef.maxRayRecursionDepth;
	pipelineInfo.layout							= pipelineBuildDef.layout.GetHandle();

	const VkPipelineCache pipelineCache = VK_NULL_HANDLE;

	VkPipeline pipelineHandle = VK_NULL_HANDLE;

	SPT_VK_CHECK(vkCreateRayTracingPipelinesKHR(VulkanRHI::GetDeviceHandle(), VK_NULL_HANDLE, pipelineCache, 1, &pipelineInfo, VulkanRHI::GetAllocationCallbacks(), &pipelineHandle));
	
	return pipelineHandle;
}

} // ray_tracing

} // helpers

//////////////////////////////////////////////////////////////////////////////////////////////////
// RHIPipeline ===================================================================================

RHIPipeline::RHIPipeline()
	: m_handle(VK_NULL_HANDLE)
	, m_pipelineType(rhi::EPipelineType::None)
{ }

void RHIPipeline::InitializeRHI(const rhi::PipelineShaderStagesDefinition& shaderStagesDef, const rhi::GraphicsPipelineDefinition& pipelineDefinition, const rhi::PipelineLayoutDefinition& layoutDefinition)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(!IsValid());

	InitializePipelineLayout(layoutDefinition);

	SPT_CHECK(!!m_layout);

	InitializeGraphicsPipeline(shaderStagesDef, pipelineDefinition, *m_layout);
}

void RHIPipeline::InitializeRHI(const rhi::RHIShaderModule& computeShaderModule, const rhi::PipelineLayoutDefinition& layoutDefinition)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(!IsValid());

	InitializePipelineLayout(layoutDefinition);

	SPT_CHECK(!!m_layout);

	InitializeComputePipeline(computeShaderModule, *m_layout);
}

void RHIPipeline::InitializeRHI(const rhi::RayTracingShadersDefinition& shadersDef, const rhi::RayTracingPipelineDefinition& pipelineDef, const rhi::PipelineLayoutDefinition& layoutDefinition)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(!IsValid());

	InitializePipelineLayout(layoutDefinition);

	SPT_CHECK(!!m_layout);

	InitializeRayTracingPipeline(shadersDef, pipelineDef, *m_layout);
}

void RHIPipeline::ReleaseRHI()
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(IsValid());

	helpers::ReleasePipelineResource(m_handle);
	m_handle = VK_NULL_HANDLE;
	m_pipelineType = rhi::EPipelineType::None;

	m_layout.reset();

	m_debugName.Reset();
}

Bool RHIPipeline::IsValid() const
{
	return m_handle != VK_NULL_HANDLE;
}

rhi::EPipelineType RHIPipeline::GetPipelineType() const
{
	return m_pipelineType;
}

rhi::DescriptorSetLayoutID RHIPipeline::GetDescriptorSetLayoutID(Uint32 dsIdx) const
{
	SPT_CHECK(IsValid());
	SPT_CHECK(!!m_layout);

	return VulkanToRHI::GetDSLayoutID(m_layout->GetDescriptorSetLayout(dsIdx));
}

void RHIPipeline::SetName(const lib::HashedString& name)
{
	SPT_CHECK(IsValid());

	m_debugName.Set(name, reinterpret_cast<Uint64>(m_handle), VK_OBJECT_TYPE_PIPELINE);
}

const lib::HashedString& RHIPipeline::GetName() const
{
	return m_debugName.Get();
}

VkPipeline RHIPipeline::GetHandle() const
{
	return m_handle;
}

const PipelineLayout& RHIPipeline::GetPipelineLayout() const
{
	SPT_CHECK(IsValid());
	SPT_CHECK(!!m_layout);
	return *m_layout;
}

void RHIPipeline::InitializePipelineLayout(const rhi::PipelineLayoutDefinition& layoutDefinition)
{
	SPT_CHECK(!m_layout);

	m_layout = VulkanRHI::GetPipelineLayoutsManager().GetOrCreatePipelineLayout(layoutDefinition);
}

void RHIPipeline::InitializeGraphicsPipeline(const rhi::PipelineShaderStagesDefinition& shaderStagesDef, const rhi::GraphicsPipelineDefinition& pipelineDefinition, const PipelineLayout& layout)
{
	SPT_PROFILER_FUNCTION();

	const helpers::gfx::GraphicsPipelineBuildDefinition pipelineBuildDefinition(shaderStagesDef, pipelineDefinition, layout);
	m_handle = helpers::gfx::BuildGraphicsPipeline(pipelineBuildDefinition);

	m_pipelineType = rhi::EPipelineType::Graphics;
}

void RHIPipeline::InitializeComputePipeline(const rhi::RHIShaderModule& computeShaderModule, const PipelineLayout& layout)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(computeShaderModule.IsValid());

	const helpers::compute::ComputePipelineBuildDefinition pipelineBuildDefinition(computeShaderModule, layout);
	m_handle = helpers::compute::BuildComputePipeline(pipelineBuildDefinition);

	m_pipelineType = rhi::EPipelineType::Compute;
}

void RHIPipeline::InitializeRayTracingPipeline(const rhi::RayTracingShadersDefinition& shadersDef, const rhi::RayTracingPipelineDefinition& pipelineDef, const PipelineLayout& layout)
{
	SPT_PROFILER_FUNCTION();

	const helpers::ray_tracing::RayTracingPipelineBuildDefinition pipelineBuildDefinition(shadersDef, pipelineDef, layout);
	m_handle = helpers::ray_tracing::BuildRayTracingPipeline(pipelineBuildDefinition);

	m_pipelineType = rhi::EPipelineType::RayTracing;
}

} // spt::vulkan