#include "VulkanRHIUtils.h"

namespace spt::vulkan
{
//////////////////////////////////////////////////////////////////////////////////////////////////
// Helpers =======================================================================================

	
static_assert(sizeof(rhi::DescriptorSetLayoutID) == sizeof(VkDescriptorSetLayout), "Couldn't store vulkan layout handle as id");

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

VkPipelineStageFlags RHIToVulkan::GetStageFlagsLegacy(rhi::EShaderStageFlags flags)
{
	VkPipelineStageFlags vulkanFlags = 0;

	if (lib::HasAllFlags(flags, rhi::EShaderStageFlags::Vertex))
	{
		lib::AddFlag(vulkanFlags, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT);
	}
	if (lib::HasAllFlags(flags, rhi::EShaderStageFlags::Fragment))
	{
		lib::AddFlag(vulkanFlags, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
	}
	if (lib::HasAllFlags(flags, rhi::EShaderStageFlags::Compute))
	{
		lib::AddFlag(vulkanFlags, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
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
    case rhi::EFragmentFormat::RGBA8_UN_Float:      return VK_FORMAT_R8G8B8A8_UNORM;
    case rhi::EFragmentFormat::BGRA8_UN_Float:      return VK_FORMAT_B8G8R8A8_UNORM;

    case rhi::EFragmentFormat::RGB8_UN_Float:       return VK_FORMAT_R8G8B8_UNORM;
    case rhi::EFragmentFormat::BGR8_UN_Float:       return VK_FORMAT_B8G8R8_UNORM;

    case rhi::EFragmentFormat::RGBA32_S_Float:      return VK_FORMAT_R32G32B32A32_SFLOAT;
    case rhi::EFragmentFormat::D32_S_Float:         return VK_FORMAT_D32_SFLOAT;

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
	case spt::rhi::EShaderStage::None:		return VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;
	case spt::rhi::EShaderStage::Vertex:	return VK_SHADER_STAGE_VERTEX_BIT;
	case spt::rhi::EShaderStage::Fragment:	return VK_SHADER_STAGE_FRAGMENT_BIT;
	case spt::rhi::EShaderStage::Compute:	return VK_SHADER_STAGE_COMPUTE_BIT;
		
	default:

		SPT_CHECK_NO_ENTRY();
		return VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;
	}
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

VkCompareOp RHIToVulkan::GetCompareOp(rhi::EDepthCompareOperation compareOperation)
{
	switch (compareOperation)
	{
	case rhi::EDepthCompareOperation::Less:					return VK_COMPARE_OP_LESS;
	case rhi::EDepthCompareOperation::LessOrEqual:			return VK_COMPARE_OP_LESS_OR_EQUAL;
	case rhi::EDepthCompareOperation::Greater:				return VK_COMPARE_OP_GREATER;
	case rhi::EDepthCompareOperation::GreaterOrEqual:		return VK_COMPARE_OP_GREATER_OR_EQUAL;
	case rhi::EDepthCompareOperation::Equal:				return VK_COMPARE_OP_EQUAL;
	case rhi::EDepthCompareOperation::NotEqual:				return VK_COMPARE_OP_NOT_EQUAL;
	case rhi::EDepthCompareOperation::Always:				return VK_COMPARE_OP_ALWAYS;
	case rhi::EDepthCompareOperation::Never:				return VK_COMPARE_OP_NEVER;

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

VkDescriptorSetLayout RHIToVulkan::GetDSLayout(rhi::DescriptorSetLayoutID layoutID)
{
	return reinterpret_cast<VkDescriptorSetLayout>(layoutID);
}

const VkDescriptorSetLayout* RHIToVulkan::GetDSLayoutsPtr(const rhi::DescriptorSetLayoutID* layoutIDs)
{
	return reinterpret_cast<const VkDescriptorSetLayout*>(layoutIDs);
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// VulkanToRHI ===================================================================================

rhi::DescriptorSetLayoutID VulkanToRHI::GetDSLayoutID(VkDescriptorSetLayout layoutHandle)
{
	return reinterpret_cast<rhi::DescriptorSetLayoutID>(layoutHandle);
}

const rhi::DescriptorSetLayoutID* VulkanToRHI::GetDSLayoutIDsPtr(VkDescriptorSetLayout layoutHandle)
{
	return reinterpret_cast<const rhi::DescriptorSetLayoutID*>(layoutHandle);
}

} // rhi::vulkan