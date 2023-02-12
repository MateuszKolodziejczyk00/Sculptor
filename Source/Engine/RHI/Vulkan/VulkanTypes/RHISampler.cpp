#include "RHISampler.h"
#include "Vulkan/VulkanRHI.h"
#include "Vulkan/VulkanRHIUtils.h"

namespace spt::vulkan
{

RHISampler::RHISampler()
	: m_handle(VK_NULL_HANDLE)
{ }

void RHISampler::InitializeRHI(const rhi::SamplerDefinition& def)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(!IsValid());

    VkSamplerCreateInfo samplerInfo
    {
        .sType                      = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .flags                      = RHIToVulkan::GetSamplerCreateFlags(def.flags),
        .magFilter                  = RHIToVulkan::GetSamplerFilterType(def.magnificationFilter),
        .minFilter                  = RHIToVulkan::GetSamplerFilterType(def.minificationFilter),
        .mipmapMode                 = RHIToVulkan::GetMipMapAddressingMode(def.mipMapAdressingMode),
        .addressModeU               = RHIToVulkan::GetAxisAddressingMode(def.addressingModeU),
        .addressModeV               = RHIToVulkan::GetAxisAddressingMode(def.addressingModeV),
        .addressModeW               = RHIToVulkan::GetAxisAddressingMode(def.addressingModeW),
        .mipLodBias                 = def.mipLodBias,
        .anisotropyEnable           = def.enableAnisotropy,
        .maxAnisotropy              = def.maxAnisotropy,
        .compareEnable              = VK_FALSE,
        .compareOp                  = VK_COMPARE_OP_MAX_ENUM,
        .minLod                     = def.minLod,
        .maxLod                     = def.maxLod < 0.f ? VK_LOD_CLAMP_NONE : def.maxLod,
        .borderColor                = RHIToVulkan::GetBorderColor(def.borderColor),
        .unnormalizedCoordinates    = def.unnormalizedCoords
	};

    VkSamplerReductionModeCreateInfo reductionModeInfo{ VK_STRUCTURE_TYPE_SAMPLER_REDUCTION_MODE_CREATE_INFO };
    reductionModeInfo.reductionMode = RHIToVulkan::GetSamplerReductionMode(def.reductionMode);

    samplerInfo.pNext = &reductionModeInfo;

	SPT_VK_CHECK(vkCreateSampler(VulkanRHI::GetDeviceHandle(), &samplerInfo, VulkanRHI::GetAllocationCallbacks(), &m_handle));
}

void RHISampler::ReleaseRHI()
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(IsValid());

	vkDestroySampler(VulkanRHI::GetDeviceHandle(), m_handle, VulkanRHI::GetAllocationCallbacks());
	m_handle = VK_NULL_HANDLE;
}

Bool RHISampler::IsValid() const
{
	return m_handle != VK_NULL_HANDLE;
}

VkSampler RHISampler::GetHandle() const
{
	return m_handle;
}

} // spt::vulkan
