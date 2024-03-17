#include "VulkanRHIUtils.h"

namespace spt::vulkan
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// RHIToVulkan ===================================================================================

VkPipelineStageFlags2 RHIToVulkan::GetStageFlags(rhi::EPipelineStage flags)
{
	VkPipelineStageFlags2 vulkanFlags = 0;

	if (lib::HasAnyFlag(flags, rhi::EPipelineStage::TopOfPipe))
	{
		lib::AddFlag(vulkanFlags, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT);
	}
	if (lib::HasAnyFlag(flags, rhi::EPipelineStage::DrawIndirect))
	{
		lib::AddFlag(vulkanFlags, VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT);
	}
	if (lib::HasAnyFlag(flags, rhi::EPipelineStage::VertexShader))
	{
		lib::AddFlag(vulkanFlags, VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT);
	}
	if (lib::HasAnyFlag(flags, rhi::EPipelineStage::FragmentShader))
	{
		lib::AddFlag(vulkanFlags, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT);
	}
	if (lib::HasAnyFlag(flags, rhi::EPipelineStage::EarlyFragmentTest))
	{
		lib::AddFlag(vulkanFlags, VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT);
	}
	if (lib::HasAnyFlag(flags, rhi::EPipelineStage::LateFragmentTest))
	{
		lib::AddFlag(vulkanFlags, VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT);
	}
	if (lib::HasAnyFlag(flags, rhi::EPipelineStage::ColorRTOutput))
	{
		lib::AddFlag(vulkanFlags, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT);
	}
	if (lib::HasAnyFlag(flags, rhi::EPipelineStage::ComputeShader))
	{
		lib::AddFlag(vulkanFlags, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT);
	}
	if (lib::HasAnyFlag(flags, rhi::EPipelineStage::Transfer))
	{
		lib::AddFlag(vulkanFlags, VK_PIPELINE_STAGE_2_TRANSFER_BIT);
	}
	if (lib::HasAnyFlag(flags, rhi::EPipelineStage::BottomOfPipe))
	{
		lib::AddFlag(vulkanFlags, VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT);
	}
	if (lib::HasAnyFlag(flags, rhi::EPipelineStage::Copy))
	{
		lib::AddFlag(vulkanFlags, VK_PIPELINE_STAGE_2_COPY_BIT);
	}
	if (lib::HasAnyFlag(flags, rhi::EPipelineStage::Blit))
	{
		lib::AddFlag(vulkanFlags, VK_PIPELINE_STAGE_2_BLIT_BIT);
	}
	if (lib::HasAnyFlag(flags, rhi::EPipelineStage::Clear))
	{
		lib::AddFlag(vulkanFlags, VK_PIPELINE_STAGE_2_CLEAR_BIT);
	}
	if (lib::HasAnyFlag(flags, rhi::EPipelineStage::IndexInput))
	{
		lib::AddFlag(vulkanFlags, VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT);
	}
	if (lib::HasAnyFlag(flags, rhi::EPipelineStage::ASBuild))
	{
		lib::AddFlag(vulkanFlags, VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR);
	}
	if (lib::HasAnyFlag(flags, rhi::EPipelineStage::RayTracingShader))
	{
		lib::AddFlag(vulkanFlags, VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR);
	}
	if (lib::HasAnyFlag(flags, rhi::EPipelineStage::Host))
	{
		lib::AddFlag(vulkanFlags, VK_PIPELINE_STAGE_2_HOST_BIT);
	}
	if (lib::HasAnyFlag(flags, rhi::EPipelineStage::ALL_GRAPHICS))
	{
		lib::AddFlag(vulkanFlags, VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT);
	}
	if (lib::HasAnyFlag(flags, rhi::EPipelineStage::ALL_TRANSFER))
	{
		lib::AddFlag(vulkanFlags, VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT);
	}
	if (lib::HasAnyFlag(flags, rhi::EPipelineStage::ALL_COMMANDS))
	{
		lib::AddFlag(vulkanFlags, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT);
	}

	return vulkanFlags;
}

VkPipelineStageFlags RHIToVulkan::GetStageFlagsLegacy(rhi::EPipelineStage flags)
{
	VkPipelineStageFlags vulkanFlags = 0;

	if (lib::HasAnyFlag(flags, rhi::EPipelineStage::TopOfPipe))
	{
		lib::AddFlag(vulkanFlags, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);
	}
	if (lib::HasAnyFlag(flags, rhi::EPipelineStage::DrawIndirect))
	{
		lib::AddFlag(vulkanFlags, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT);
	}
	if (lib::HasAnyFlag(flags, rhi::EPipelineStage::VertexShader))
	{
		lib::AddFlag(vulkanFlags, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT);
	}
	if (lib::HasAnyFlag(flags, rhi::EPipelineStage::FragmentShader))
	{
		lib::AddFlag(vulkanFlags, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
	}
	if (lib::HasAnyFlag(flags, rhi::EPipelineStage::EarlyFragmentTest))
	{
		lib::AddFlag(vulkanFlags, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT);
	}
	if (lib::HasAnyFlag(flags, rhi::EPipelineStage::LateFragmentTest))
	{
		lib::AddFlag(vulkanFlags, VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT);
	}
	if (lib::HasAnyFlag(flags, rhi::EPipelineStage::ColorRTOutput))
	{
		lib::AddFlag(vulkanFlags, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
	}
	if (lib::HasAnyFlag(flags, rhi::EPipelineStage::ComputeShader))
	{
		lib::AddFlag(vulkanFlags, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
	}
	if (lib::HasAnyFlag(flags, rhi::EPipelineStage::Transfer))
	{
		lib::AddFlag(vulkanFlags, VK_PIPELINE_STAGE_TRANSFER_BIT);
	}
	if (lib::HasAnyFlag(flags, rhi::EPipelineStage::BottomOfPipe))
	{
		lib::AddFlag(vulkanFlags, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
	}
	if (lib::HasAnyFlag(flags, rhi::EPipelineStage::Copy))
	{
		SPT_CHECK_NO_ENTRY(); // No corresponding legacy flag
	}
	if (lib::HasAnyFlag(flags, rhi::EPipelineStage::Blit))
	{
		SPT_CHECK_NO_ENTRY(); // No corresponding legacy flag
	}
	if (lib::HasAnyFlag(flags, rhi::EPipelineStage::Clear))
	{
		SPT_CHECK_NO_ENTRY(); // No corresponding legacy flag
	}
	if (lib::HasAnyFlag(flags, rhi::EPipelineStage::IndexInput))
	{
		SPT_CHECK_NO_ENTRY(); // No corresponding legacy flag
	}
	if (lib::HasAnyFlag(flags, rhi::EPipelineStage::ASBuild))
	{
		lib::AddFlag(vulkanFlags, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR);
	}
	if (lib::HasAnyFlag(flags, rhi::EPipelineStage::RayTracingShader))
	{
		lib::AddFlag(vulkanFlags, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR);
	}
	if (lib::HasAnyFlag(flags, rhi::EPipelineStage::Host))
	{
		lib::AddFlag(vulkanFlags, VK_PIPELINE_STAGE_HOST_BIT);
	}
	if (lib::HasAnyFlag(flags, rhi::EPipelineStage::ALL_GRAPHICS))
	{
		lib::AddFlag(vulkanFlags, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT);
	}
	if (lib::HasAnyFlag(flags, rhi::EPipelineStage::ALL_TRANSFER))
	{
		lib::AddFlag(vulkanFlags, VK_PIPELINE_STAGE_TRANSFER_BIT);
	}
	if (lib::HasAnyFlag(flags, rhi::EPipelineStage::ALL_COMMANDS))
	{
		lib::AddFlag(vulkanFlags, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
	}

	return vulkanFlags;
}

VkImageLayout RHIToVulkan::GetImageLayout(rhi::ETextureLayout layout)
{
	switch (layout)
	{
	case rhi::ETextureLayout::Undefined:						return VK_IMAGE_LAYOUT_UNDEFINED;
	case rhi::ETextureLayout::General:							return VK_IMAGE_LAYOUT_GENERAL;
	case rhi::ETextureLayout::ColorRTOptimal:					return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	case rhi::ETextureLayout::DepthRTOptimal:					return VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
	case rhi::ETextureLayout::DepthStencilRTOptimal:			return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	case rhi::ETextureLayout::DepthRTStencilReadOptimal:		return VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL;
	case rhi::ETextureLayout::DepthReadOnlyStencilRTOptimal:	return VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL;
	case rhi::ETextureLayout::TransferSrcOptimal:				return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	case rhi::ETextureLayout::TransferDstOptimal:				return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	case rhi::ETextureLayout::DepthReadOnlyOptimal:				return VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL;
	case rhi::ETextureLayout::DepthStencilReadOnlyOptimal:		return VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
	case rhi::ETextureLayout::ColorReadOnlyOptimal:				return VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL;
	case rhi::ETextureLayout::PresentSrc:						return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	case rhi::ETextureLayout::ReadOnlyOptimal:					return VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL;
	case rhi::ETextureLayout::RenderTargetOptimal:				return VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
	}

	SPT_CHECK_NO_ENTRY();
	return VK_IMAGE_LAYOUT_UNDEFINED;
}

VkFormat RHIToVulkan::GetVulkanFormat(rhi::EFragmentFormat format)
{
    switch (format)
    {
	case rhi::EFragmentFormat::None:					return VK_FORMAT_UNDEFINED;

    case rhi::EFragmentFormat::R8_UN_Float:				return VK_FORMAT_R8_UNORM;
    case rhi::EFragmentFormat::R8_U_Int:				return VK_FORMAT_R8_UINT;
    case rhi::EFragmentFormat::R16_UN_Float:			return VK_FORMAT_R16_UNORM;
    case rhi::EFragmentFormat::R16_S_Float:			    return VK_FORMAT_R16_SFLOAT;
    case rhi::EFragmentFormat::R32_S_Float:				return VK_FORMAT_R32_SFLOAT;
	case rhi::EFragmentFormat::R32_U_Int:				return VK_FORMAT_R32_UINT;
														
    case rhi::EFragmentFormat::RG8_UN_Float:			return VK_FORMAT_R8G8_UNORM;
    case rhi::EFragmentFormat::RG16_UN_Float:			return VK_FORMAT_R16G16_UNORM;
    case rhi::EFragmentFormat::RG16_SN_Float:			return VK_FORMAT_R16G16_SNORM;
    case rhi::EFragmentFormat::RG16_S_Float:			return VK_FORMAT_R16G16_SFLOAT;
    case rhi::EFragmentFormat::RG32_S_Float:			return VK_FORMAT_R32G32_SFLOAT;
														
    case rhi::EFragmentFormat::RGB8_UN_Float:			return VK_FORMAT_R8G8B8_UNORM;
    case rhi::EFragmentFormat::BGR8_UN_Float:			return VK_FORMAT_B8G8R8_UNORM;
    case rhi::EFragmentFormat::RGB16_UN_Float:			return VK_FORMAT_R16G16B16_UNORM;
    case rhi::EFragmentFormat::RGB32_S_Float:			return VK_FORMAT_R32G32B32_SFLOAT;
    
	case rhi::EFragmentFormat::B10G11R11_U_Float:		return VK_FORMAT_B10G11R11_UFLOAT_PACK32;
    case rhi::EFragmentFormat::A2B10G10R10_UN_Float:	return VK_FORMAT_A2B10G10R10_UNORM_PACK32;
													
    case rhi::EFragmentFormat::RGBA8_UN_Float:			return VK_FORMAT_R8G8B8A8_UNORM;
    case rhi::EFragmentFormat::BGRA8_UN_Float:			return VK_FORMAT_B8G8R8A8_UNORM;
    case rhi::EFragmentFormat::RGBA16_UN_Float:			return VK_FORMAT_R16G16B16A16_UNORM;
    case rhi::EFragmentFormat::RGBA16_S_Float:			return VK_FORMAT_R16G16B16A16_SFLOAT;
                                               
    case rhi::EFragmentFormat::RGBA32_S_Float:			return VK_FORMAT_R32G32B32A32_SFLOAT;
    
	case rhi::EFragmentFormat::D16_UN_Float:			return VK_FORMAT_D16_UNORM;
    case rhi::EFragmentFormat::D32_S_Float:				return VK_FORMAT_D32_SFLOAT;

    default:

        SPT_CHECK_NO_ENTRY();
        return VK_FORMAT_UNDEFINED;
    }
}

VkImageAspectFlags RHIToVulkan::GetAspectFlags(rhi::ETextureAspect flags)
{
	VkImageAspectFlags vulkanAspect = 0;

	if (lib::HasAnyFlag(flags, rhi::ETextureAspect::Color))
	{
		lib::AddFlag(vulkanAspect, VK_IMAGE_ASPECT_COLOR_BIT);
	}
	if (lib::HasAnyFlag(flags, rhi::ETextureAspect::Depth))
	{
		lib::AddFlag(vulkanAspect, VK_IMAGE_ASPECT_DEPTH_BIT);
	}
	if (lib::HasAnyFlag(flags, rhi::ETextureAspect::Stencil))
	{
		lib::AddFlag(vulkanAspect, VK_IMAGE_ASPECT_STENCIL_BIT);
	}

	return vulkanAspect;
}

VkRenderingFlags RHIToVulkan::GetRenderingFlags(rhi::ERenderingFlags flags)
{
	VkRenderingFlags vulkanFlags = 0;

	if (lib::HasAnyFlag(flags, rhi::ERenderingFlags::ContentsSecondaryCmdBuffers))
	{
		lib::AddFlag(vulkanFlags, VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT);
	}
	if (lib::HasAnyFlag(flags, rhi::ERenderingFlags::Resuming))
	{
		lib::AddFlag(vulkanFlags, VK_RENDERING_RESUMING_BIT);
	}

	return vulkanFlags;
}

VkAttachmentLoadOp RHIToVulkan::GetLoadOp(rhi::ERTLoadOperation loadOp)
{
	switch (loadOp)
	{
	case rhi::ERTLoadOperation::Load:			return VK_ATTACHMENT_LOAD_OP_LOAD;
	case rhi::ERTLoadOperation::Clear:			return VK_ATTACHMENT_LOAD_OP_CLEAR;
	case rhi::ERTLoadOperation::DontCare:		return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	}

	SPT_CHECK_NO_ENTRY();
	return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
}

VkAttachmentStoreOp RHIToVulkan::GetStoreOp(rhi::ERTStoreOperation storeOp)
{
	switch (storeOp)
	{
	case rhi::ERTStoreOperation::Store:			return VK_ATTACHMENT_STORE_OP_STORE;
	case rhi::ERTStoreOperation::DontCare:		return VK_ATTACHMENT_STORE_OP_DONT_CARE;
	}

	SPT_CHECK_NO_ENTRY();
	return VK_ATTACHMENT_STORE_OP_DONT_CARE;
}

VkResolveModeFlagBits RHIToVulkan::GetResolveMode(rhi::ERTResolveMode resolveMode)
{
	switch (resolveMode)
	{
	case rhi::ERTResolveMode::Average:			return VK_RESOLVE_MODE_AVERAGE_BIT;
	case rhi::ERTResolveMode::Min:				return VK_RESOLVE_MODE_MIN_BIT;
	case rhi::ERTResolveMode::Max:				return VK_RESOLVE_MODE_MAX_BIT;
	case rhi::ERTResolveMode::SampleZero:		return VK_RESOLVE_MODE_SAMPLE_ZERO_BIT;
	}

	SPT_CHECK_NO_ENTRY();
	return VK_RESOLVE_MODE_AVERAGE_BIT;
}

VkDescriptorType RHIToVulkan::GetDescriptorType(rhi::EDescriptorType descriptorType)
{
	switch (descriptorType)
	{
	case spt::rhi::EDescriptorType::Sampler:						return VK_DESCRIPTOR_TYPE_SAMPLER;
	case spt::rhi::EDescriptorType::CombinedTextureSampler:			return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	case spt::rhi::EDescriptorType::SampledTexture:					return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
	case spt::rhi::EDescriptorType::StorageTexture:					return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	case spt::rhi::EDescriptorType::UniformTexelBuffer:				return VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
	case spt::rhi::EDescriptorType::StorageTexelBuffer:				return VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
	case spt::rhi::EDescriptorType::UniformBuffer:					return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	case spt::rhi::EDescriptorType::StorageBuffer:					return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	case spt::rhi::EDescriptorType::UniformBufferDynamicOffset:		return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
	case spt::rhi::EDescriptorType::StorageBufferDynamicOffset:		return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
	case spt::rhi::EDescriptorType::AccelerationStructure:			return VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;

	default:

		SPT_CHECK_NO_ENTRY();
		return VK_DESCRIPTOR_TYPE_MAX_ENUM;
	}
}

VkDescriptorBindingFlags RHIToVulkan::GetBindingFlags(rhi::EDescriptorSetBindingFlags bindingFlags)
{
	VkDescriptorBindingFlags vulkanFlags = 0;

	if (lib::HasAnyFlag(bindingFlags, rhi::EDescriptorSetBindingFlags::UpdateAfterBind))
	{
		lib::AddFlag(vulkanFlags, VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT);
	}
	if (lib::HasAnyFlag(bindingFlags, rhi::EDescriptorSetBindingFlags::PartiallyBound))
	{
		lib::AddFlag(vulkanFlags, VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT);
	}

	return vulkanFlags;
}

VkDescriptorSetLayoutCreateFlags RHIToVulkan::GetDescriptorSetFlags(rhi::EDescriptorSetFlags dsFlags)
{
	VkDescriptorSetLayoutCreateFlags vulkanFlags = 0;


	return vulkanFlags;
}

VkShaderStageFlagBits RHIToVulkan::GetShaderStage(rhi::EShaderStage stage)
{
	switch (stage)
	{
	case rhi::EShaderStage::None:			return VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;
	case rhi::EShaderStage::Vertex:			return VK_SHADER_STAGE_VERTEX_BIT;
	case rhi::EShaderStage::Task:			return VK_SHADER_STAGE_TASK_BIT_EXT;
	case rhi::EShaderStage::Mesh:			return VK_SHADER_STAGE_MESH_BIT_EXT;
	case rhi::EShaderStage::Fragment:		return VK_SHADER_STAGE_FRAGMENT_BIT;
	case rhi::EShaderStage::Compute:		return VK_SHADER_STAGE_COMPUTE_BIT;
	case rhi::EShaderStage::RTGeneration:	return VK_SHADER_STAGE_RAYGEN_BIT_KHR;
	case rhi::EShaderStage::RTAnyHit:		return VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
	case rhi::EShaderStage::RTClosestHit:	return VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
	case rhi::EShaderStage::RTMiss:			return VK_SHADER_STAGE_MISS_BIT_KHR;
	case rhi::EShaderStage::RTIntersection:	return VK_SHADER_STAGE_INTERSECTION_BIT_KHR;

	default:

		SPT_CHECK_NO_ENTRY();
		return VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;
	}
}

VkShaderStageFlags RHIToVulkan::GetShaderStages(rhi::EShaderStageFlags stages)
{
	VkShaderStageFlags flags{};

	if (lib::HasAnyFlag(stages, rhi::EShaderStageFlags::Vertex))
	{
		lib::AddFlag(flags, VK_SHADER_STAGE_VERTEX_BIT);
	}
	if (lib::HasAnyFlag(stages, rhi::EShaderStageFlags::Task))
	{
		lib::AddFlag(flags, VK_SHADER_STAGE_TASK_BIT_EXT);
	}
	if (lib::HasAnyFlag(stages, rhi::EShaderStageFlags::Mesh))
	{
		lib::AddFlag(flags, VK_SHADER_STAGE_MESH_BIT_EXT);
	}
	if (lib::HasAnyFlag(stages, rhi::EShaderStageFlags::Fragment))
	{
		lib::AddFlag(flags, VK_SHADER_STAGE_FRAGMENT_BIT);
	}
	if (lib::HasAnyFlag(stages, rhi::EShaderStageFlags::Compute))
	{
		lib::AddFlag(flags, VK_SHADER_STAGE_COMPUTE_BIT);
	}
	if (lib::HasAnyFlag(stages, rhi::EShaderStageFlags::RTGeneration))
	{
		lib::AddFlag(flags, VK_SHADER_STAGE_RAYGEN_BIT_KHR);
	}
	if (lib::HasAnyFlag(stages, rhi::EShaderStageFlags::RTAnyHit))
	{
		lib::AddFlag(flags, VK_SHADER_STAGE_ANY_HIT_BIT_KHR);
	}
	if (lib::HasAnyFlag(stages, rhi::EShaderStageFlags::RTClosestHit))
	{
		lib::AddFlag(flags, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR);
	}
	if (lib::HasAnyFlag(stages, rhi::EShaderStageFlags::RTMiss))
	{
		lib::AddFlag(flags, VK_SHADER_STAGE_MISS_BIT_KHR);
	}
	if (lib::HasAnyFlag(stages, rhi::EShaderStageFlags::RTIntersection))
	{
		lib::AddFlag(flags, VK_SHADER_STAGE_INTERSECTION_BIT_KHR);
	}

	return flags;
}

VkPrimitiveTopology RHIToVulkan::GetPrimitiveTopology(rhi::EPrimitiveTopology topology)
{
	switch (topology)
	{
	case rhi::EPrimitiveTopology::PointList:		return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
	case rhi::EPrimitiveTopology::LineList:			return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
	case rhi::EPrimitiveTopology::LineStrip:		return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
	case rhi::EPrimitiveTopology::TriangleList:		return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	case rhi::EPrimitiveTopology::TriangleStrip:	return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
	case rhi::EPrimitiveTopology::TriangleFan:		return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN;

	default:

		SPT_CHECK_NO_ENTRY();
		return VK_PRIMITIVE_TOPOLOGY_MAX_ENUM;
	}
}

VkCullModeFlags RHIToVulkan::GetCullMode(rhi::ECullMode cullMode)
{
	switch (cullMode)
	{
	case rhi::ECullMode::Back:			return VK_CULL_MODE_BACK_BIT;
	case rhi::ECullMode::Front:			return VK_CULL_MODE_FRONT_BIT;
	case rhi::ECullMode::BackAndFront:	return VK_CULL_MODE_FRONT_AND_BACK;
	case rhi::ECullMode::None:			return VK_CULL_MODE_NONE;

	default:

		SPT_CHECK_NO_ENTRY();
		return VK_CULL_MODE_FLAG_BITS_MAX_ENUM;
	}
}

VkPolygonMode RHIToVulkan::GetPolygonMode(rhi::EPolygonMode polygonMode)
{
	switch (polygonMode)
	{
	case rhi::EPolygonMode::Fill:			return VK_POLYGON_MODE_FILL;
	case rhi::EPolygonMode::Wireframe:		return VK_POLYGON_MODE_LINE;
		
	default:

		SPT_CHECK_NO_ENTRY();
		return VK_POLYGON_MODE_MAX_ENUM;
	}
}

VkSampleCountFlagBits RHIToVulkan::GetSampleCount(Uint32 samplesNum)
{
	switch (samplesNum)
	{
	case 1:		return VK_SAMPLE_COUNT_1_BIT;
	case 2:		return VK_SAMPLE_COUNT_2_BIT;
	case 4:		return VK_SAMPLE_COUNT_4_BIT;
	case 8:		return VK_SAMPLE_COUNT_8_BIT;
	case 16:	return VK_SAMPLE_COUNT_16_BIT;
	case 32:	return VK_SAMPLE_COUNT_32_BIT;
	case 64:	return VK_SAMPLE_COUNT_64_BIT;

	default:

		SPT_CHECK_NO_ENTRY();
		return VK_SAMPLE_COUNT_FLAG_BITS_MAX_ENUM;
	}
}

VkCompareOp RHIToVulkan::GetCompareOp(rhi::ECompareOp compareOperation)
{
	switch (compareOperation)
	{
	case rhi::ECompareOp::Less:					return VK_COMPARE_OP_LESS;
	case rhi::ECompareOp::LessOrEqual:			return VK_COMPARE_OP_LESS_OR_EQUAL;
	case rhi::ECompareOp::Greater:				return VK_COMPARE_OP_GREATER;
	case rhi::ECompareOp::GreaterOrEqual:		return VK_COMPARE_OP_GREATER_OR_EQUAL;
	case rhi::ECompareOp::Equal:				return VK_COMPARE_OP_EQUAL;
	case rhi::ECompareOp::NotEqual:				return VK_COMPARE_OP_NOT_EQUAL;
	case rhi::ECompareOp::Always:				return VK_COMPARE_OP_ALWAYS;
	case rhi::ECompareOp::Never:				return VK_COMPARE_OP_NEVER;

	default:

		SPT_CHECK_NO_ENTRY();
		return VK_COMPARE_OP_MAX_ENUM;
	}
}

VkColorComponentFlags RHIToVulkan::GetColorComponentFlags(rhi::ERenderTargetComponentFlags componentFlags)
{
	VkColorComponentFlags flags = 0;

	if (lib::HasAnyFlag(componentFlags, rhi::ERenderTargetComponentFlags::R))
	{
		lib::AddFlag(flags, VK_COLOR_COMPONENT_R_BIT);
	}
	if (lib::HasAnyFlag(componentFlags, rhi::ERenderTargetComponentFlags::G))
	{
		lib::AddFlag(flags, VK_COLOR_COMPONENT_G_BIT);
	}
	if (lib::HasAnyFlag(componentFlags, rhi::ERenderTargetComponentFlags::B))
	{
		lib::AddFlag(flags, VK_COLOR_COMPONENT_B_BIT);
	}
	if (lib::HasAnyFlag(componentFlags, rhi::ERenderTargetComponentFlags::A))
	{
		lib::AddFlag(flags, VK_COLOR_COMPONENT_A_BIT);
	}

	return flags;
}

VkSamplerCreateFlags RHIToVulkan::GetSamplerCreateFlags(rhi::ESamplerFlags flags)
{
	return 0;
}

VkFilter RHIToVulkan::GetSamplerFilterType(rhi::ESamplerFilterType type)
{
	switch (type)
	{
	case rhi::ESamplerFilterType::Nearest:	return VK_FILTER_NEAREST;
	case rhi::ESamplerFilterType::Linear:	return VK_FILTER_LINEAR;

	default:

		SPT_CHECK_NO_ENTRY();
		return VK_FILTER_MAX_ENUM;
	}
}

VkSamplerMipmapMode RHIToVulkan::GetMipMapAddressingMode(rhi::EMipMapAddressingMode mode)
{
	switch (mode)
	{
	case rhi::EMipMapAddressingMode::Nearest:	return VK_SAMPLER_MIPMAP_MODE_NEAREST;
	case rhi::EMipMapAddressingMode::Linear:	return VK_SAMPLER_MIPMAP_MODE_LINEAR;

	default:

		SPT_CHECK_NO_ENTRY();
		return VK_SAMPLER_MIPMAP_MODE_MAX_ENUM;
	}
}

VkSamplerAddressMode RHIToVulkan::GetAxisAddressingMode(rhi::EAxisAddressingMode mode)
{
	switch (mode)
	{
	case rhi::EAxisAddressingMode::Repeat:				return VK_SAMPLER_ADDRESS_MODE_REPEAT;
	case rhi::EAxisAddressingMode::MirroredRepeat:		return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
	case rhi::EAxisAddressingMode::ClampToEdge: 		return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	case rhi::EAxisAddressingMode::ClampToBorder:		return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
	case rhi::EAxisAddressingMode::MorroredClampToEdge:	return VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;

	default:

		SPT_CHECK_NO_ENTRY();
		return VK_SAMPLER_ADDRESS_MODE_MAX_ENUM;
	}
}

VkSamplerReductionMode RHIToVulkan::GetSamplerReductionMode(rhi::ESamplerReductionMode mode)
{
	switch (mode)
	{
	case spt::rhi::ESamplerReductionMode::WeightedAverage:		return VK_SAMPLER_REDUCTION_MODE_WEIGHTED_AVERAGE;
	case spt::rhi::ESamplerReductionMode::Min:					return VK_SAMPLER_REDUCTION_MODE_MIN;
	case spt::rhi::ESamplerReductionMode::Max:					return VK_SAMPLER_REDUCTION_MODE_MAX;

	default:

		SPT_CHECK_NO_ENTRY();
		return VK_SAMPLER_REDUCTION_MODE_MAX_ENUM;
	}
}

VkBorderColor RHIToVulkan::GetBorderColor(rhi::EBorderColor color)
{
	switch (color)
	{
	case rhi::EBorderColor::FloatTransparentBlack:	return VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
	case rhi::EBorderColor::IntTransparentBlack:	return VK_BORDER_COLOR_INT_TRANSPARENT_BLACK;
	case rhi::EBorderColor::FloatOpaqueBlack:		return VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
	case rhi::EBorderColor::IntOpaqueBlack:			return VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	case rhi::EBorderColor::FloatOpaqueWhite:		return VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
	case rhi::EBorderColor::IntOpaqueWhite:			return VK_BORDER_COLOR_INT_OPAQUE_WHITE;

	default:

		SPT_CHECK_NO_ENTRY();
		return VK_BORDER_COLOR_MAX_ENUM;
	}
}

VkEventCreateFlags RHIToVulkan::GetEventFlags(rhi::EEventFlags eventFlags)
{
	VkEventCreateFlags flags = 0;

	if (lib::HasAnyFlag(eventFlags, rhi::EEventFlags::GPUOnly))
	{
		lib::AddFlag(flags, VK_EVENT_CREATE_DEVICE_ONLY_BIT);
	}

	return flags;
}

VkGeometryFlagsKHR RHIToVulkan::GetGeometryFlags(rhi::EGeometryFlags geometryFlags)
{
	VkGeometryFlagsKHR flags = 0;

	if (lib::HasAnyFlag(geometryFlags, rhi::EGeometryFlags::Opaque))
	{
		lib::AddFlag(flags, VK_GEOMETRY_OPAQUE_BIT_KHR);
	}

	return flags;
}

VkGeometryInstanceFlagsKHR RHIToVulkan::GetGeometryInstanceFlags(rhi::ETLASInstanceFlags instanceFlags)
{
	VkGeometryInstanceFlagsKHR flags = 0;

	if (lib::HasAnyFlag(instanceFlags, rhi::ETLASInstanceFlags::ForceNoOpaque))
	{
		lib::AddFlag(flags, VK_GEOMETRY_INSTANCE_FORCE_NO_OPAQUE_BIT_KHR);
	}
	if (lib::HasAnyFlag(instanceFlags, rhi::ETLASInstanceFlags::ForceOpaque))
	{
		lib::AddFlag(flags, VK_GEOMETRY_INSTANCE_FORCE_OPAQUE_BIT_KHR);
	}

	return flags;
}

VkQueryType RHIToVulkan::GetQueryType(rhi::EQueryType queryType)
{
	switch (queryType)
	{
	case rhi::EQueryType::Timestamp:	return VK_QUERY_TYPE_TIMESTAMP;
	case rhi::EQueryType::Statistics:	return VK_QUERY_TYPE_PIPELINE_STATISTICS;

	default:

		SPT_CHECK_NO_ENTRY();
		return VK_QUERY_TYPE_MAX_ENUM;
	}
}

VkQueryPipelineStatisticFlags RHIToVulkan::GetPipelineStatistic(rhi::EQueryStatisticsType statisticsType)
{
	VkQueryPipelineStatisticFlags flags = 0;

	if (lib::HasAnyFlag(statisticsType, rhi::EQueryStatisticsType::IAVertices))
	{
		lib::AddFlag(flags, VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT);
	}
	if (lib::HasAnyFlag(statisticsType, rhi::EQueryStatisticsType::IAPrimitives))
	{
		lib::AddFlag(flags, VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT);
	}
	if (lib::HasAnyFlag(statisticsType, rhi::EQueryStatisticsType::VSInvocations))
	{
		lib::AddFlag(flags, VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT);
	}
	if (lib::HasAnyFlag(statisticsType, rhi::EQueryStatisticsType::FSInvocations))
	{
		lib::AddFlag(flags, VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT);
	}
	if (lib::HasAnyFlag(statisticsType, rhi::EQueryStatisticsType::CSInvocations))
	{
		lib::AddFlag(flags, VK_QUERY_PIPELINE_STATISTIC_COMPUTE_SHADER_INVOCATIONS_BIT);
	}

	return flags;
}

} // rhi::vulkan
