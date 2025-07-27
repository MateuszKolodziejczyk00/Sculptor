#include "RHISampler.h"
#include "Vulkan/VulkanRHI.h"
#include "Vulkan/VulkanRHIUtils.h"
#include "Vulkan/Device/LogicalDevice.h"

namespace spt::vulkan
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// RHISamplerReleaseTicket =======================================================================

void RHISamplerReleaseTicket::ExecuteReleaseRHI()
{
	if (handle.IsValid())
	{
		vkDestroySampler(VulkanRHI::GetDeviceHandle(), handle.GetValue(), VulkanRHI::GetAllocationCallbacks());
		handle.Reset();
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// RHISampler ====================================================================================

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
	RHISamplerReleaseTicket releaseTicket = DeferredReleaseRHI();
	releaseTicket.ExecuteReleaseRHI();
}

RHISamplerReleaseTicket RHISampler::DeferredReleaseRHI()
{
	SPT_CHECK(IsValid());

	RHISamplerReleaseTicket releaseTicket;
	releaseTicket.handle = m_handle;

	m_handle = VK_NULL_HANDLE;

	SPT_CHECK(!IsValid());

	return releaseTicket;
}

Bool RHISampler::IsValid() const
{
	return m_handle != VK_NULL_HANDLE;
}

void RHISampler::CopyDescriptor(Byte* dst) const
{
	SPT_CHECK(IsValid());

	const VkSampler samplerHandle = GetHandle();

	VkDescriptorGetInfoEXT info{ VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT };
	info.type          = VK_DESCRIPTOR_TYPE_SAMPLER;
	info.data.pSampler = &samplerHandle;

	const LogicalDevice& device = VulkanRHI::GetLogicalDevice();

	vkGetDescriptorEXT(device.GetHandle(), &info, device.GetDescriptorProps().StrideFor(rhi::EDescriptorType::Sampler), dst);
}

VkSampler RHISampler::GetHandle() const
{
	return m_handle;
}

} // spt::vulkan
