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

    VkSamplerCreateInfo samplerInfo{ VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
    samplerInfo.flags                      = RHIToVulkan::GetSamplerCreateFlags(def.flags);
    samplerInfo.magFilter                  = RHIToVulkan::GetSamplerFilterType(def.magnificationFilter);
    samplerInfo.minFilter                  = RHIToVulkan::GetSamplerFilterType(def.minificationFilter);
    samplerInfo.mipmapMode                 = RHIToVulkan::GetMipMapAddressingMode(def.mipMapAdressingMode);
    samplerInfo.addressModeU               = RHIToVulkan::GetAxisAddressingMode(def.addressingModeU);
    samplerInfo.addressModeV               = RHIToVulkan::GetAxisAddressingMode(def.addressingModeV);
    samplerInfo.addressModeW               = RHIToVulkan::GetAxisAddressingMode(def.addressingModeW);
    samplerInfo.mipLodBias                 = def.mipLodBias;
    samplerInfo.anisotropyEnable           = def.enableAnisotropy;
    samplerInfo.maxAnisotropy              = def.maxAnisotropy;
    samplerInfo.minLod                     = def.minLod;
    samplerInfo.maxLod                     = def.maxLod < 0.f ? VK_LOD_CLAMP_NONE : def.maxLod;
    samplerInfo.borderColor                = RHIToVulkan::GetBorderColor(def.borderColor);
    samplerInfo.unnormalizedCoordinates    = def.unnormalizedCoords;
    
    if (def.compareOp != rhi::ECompareOp::None)
    {
        samplerInfo.compareEnable   = VK_TRUE;
        samplerInfo.compareOp       = RHIToVulkan::GetCompareOp(def.compareOp);
    }

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
