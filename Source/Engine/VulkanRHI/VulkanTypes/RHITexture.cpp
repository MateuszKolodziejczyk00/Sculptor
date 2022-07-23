#include "RHITexture.h"
#include "VulkanRHI.h"
#include "Memory/MemoryManager.h"


namespace spt::vulkan
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// Helpers =======================================================================================

namespace priv
{

VkImageType SelectImageType(const rhi::TextureDefinition& definition)
{
    if (definition.m_resolution.z() > 1)
    {
        return VK_IMAGE_TYPE_3D;
    }
    else if (definition.m_resolution.y() > 1)
    {
        return VK_IMAGE_TYPE_2D;
    }
    else
    {
        return VK_IMAGE_TYPE_1D;
    }
}

VkFormat SelectFormat(const rhi::TextureDefinition& definition)
{
    switch (definition.m_format)
    {
    case rhi::EFragmentFormat::RGBA8_UN_Float:      return VK_FORMAT_R8G8B8A8_UNORM;
    case rhi::EFragmentFormat::RGBA32_S_Float:      return VK_FORMAT_R32G32B32A32_SFLOAT;
    case rhi::EFragmentFormat::D32_S_Float:         return VK_FORMAT_D32_SFLOAT;
    }

    SPT_CHECK_NO_ENTRY();
    return VK_FORMAT_UNDEFINED;
}

VkImageUsageFlags GetVulkanTextureUsageFlags(Flags32 usageFlags)
{
    VkImageUsageFlags vulkanFlags;

    if ((usageFlags & rhi::ETextureUsage::TransferSource) != 0)
    {
        vulkanFlags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    }
    if ((usageFlags & rhi::ETextureUsage::TransferDest) != 0)
    {
        vulkanFlags |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    }
	if ((usageFlags & rhi::ETextureUsage::SampledTexture) != 0)
    {
        vulkanFlags |= VK_IMAGE_USAGE_SAMPLED_BIT;
    }
	if ((usageFlags & rhi::ETextureUsage::StorageTexture) != 0)
    {
        vulkanFlags |= VK_IMAGE_USAGE_STORAGE_BIT;
    }
	if ((usageFlags & rhi::ETextureUsage::ColorRT) != 0)
    {
        vulkanFlags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    }
	if ((usageFlags & rhi::ETextureUsage::DepthSetncilRT) != 0)
    {
        vulkanFlags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    }
	if ((usageFlags & rhi::ETextureUsage::TransientRT) != 0)
    {
        vulkanFlags |= VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;
    }
	if ((usageFlags & rhi::ETextureUsage::InputRT) != 0)
    {
        vulkanFlags |= VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
    }
	if ((usageFlags & rhi::ETextureUsage::ShadingRateTexture) != 0)
    {
        vulkanFlags |= VK_IMAGE_USAGE_SHADING_RATE_IMAGE_BIT_NV;
    }
	if ((usageFlags & rhi::ETextureUsage::FragmentDensityMap) != 0)
    {
        vulkanFlags |= VK_IMAGE_USAGE_FRAGMENT_DENSITY_MAP_BIT_EXT;
    }

    return vulkanFlags;
}

VkSampleCountFlagBits GetVulkanSampleCountFlag(Uint32 samples)
{
    switch (samples)
    {
    case 1:         return VK_SAMPLE_COUNT_1_BIT;
    case 2:         return VK_SAMPLE_COUNT_2_BIT;
    case 4:         return VK_SAMPLE_COUNT_4_BIT;
    case 8:         return VK_SAMPLE_COUNT_8_BIT;
    case 16:        return VK_SAMPLE_COUNT_16_BIT;
    case 32:        return VK_SAMPLE_COUNT_32_BIT;
    case 64:        return VK_SAMPLE_COUNT_64_BIT;
    }

    SPT_CHECK_NO_ENTRY();
    return VK_SAMPLE_COUNT_1_BIT;
}

}

//////////////////////////////////////////////////////////////////////////////////////////////////
// RHITexture ====================================================================================

RHITexture::RHITexture()
    : m_imageHandle(VK_NULL_HANDLE)
    , m_allocation(VK_NULL_HANDLE)
{ }

void RHITexture::InitializeRHI(const rhi::TextureDefinition& definition, const rhi::RHIAllocationInfo& allocation)
{
	SPT_PROFILE_FUNCTION();

    m_definition = definition;

	VkImageCreateInfo imageInfo{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
    imageInfo.flags = 0;
    imageInfo.imageType = priv::SelectImageType(definition);
    imageInfo.format = priv::SelectFormat(definition);
    imageInfo.extent = { definition.m_resolution.x(), definition.m_resolution.y(), definition.m_resolution.z() };
    imageInfo.mipLevels = definition.m_mipLevels;
    imageInfo.arrayLayers = definition.m_arrayLayers;
    imageInfo.samples = priv::GetVulkanSampleCountFlag(definition.m_samples);
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = priv::GetVulkanTextureUsageFlags(definition.m_usage);
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	const VmaAllocationCreateInfo allocationInfo = VulkanRHI::GetMemoryManager().CreateAllocationInfo(allocation);

	SPT_VK_CHECK(vmaCreateImage(VulkanRHI::GetAllocatorHandle(), &imageInfo, &allocationInfo, &m_imageHandle, &m_allocation, nullptr));
}

void RHITexture::ReleaseRHI()
{
    SPT_PROFILE_FUNCTION();

    SPT_CHECK(!!IsValid());

    vmaDestroyImage(VulkanRHI::GetAllocatorHandle(), m_imageHandle, m_allocation);

    m_imageHandle = VK_NULL_HANDLE;
    m_allocation = VK_NULL_HANDLE;
    m_name.Reset();
}

Bool RHITexture::IsValid() const
{
    return !!m_imageHandle;
}

const rhi::TextureDefinition& RHITexture::GetDefinition() const
{
    return m_definition;
}

VkImage RHITexture::GetHandle() const
{
    return m_imageHandle;
}

void RHITexture::SetName(const lib::HashedString& name)
{
    m_name.Set(name, reinterpret_cast<Uint64>(m_imageHandle), VK_OBJECT_TYPE_IMAGE);
}

const lib::HashedString& RHITexture::GetName() const
{
    return m_name.Get();
}

}