#include "RHITexture.h"
#include "VulkanRHI.h"
#include "Memory/MemoryManager.h"


namespace spt::vulkan
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// Helpers =======================================================================================

namespace priv
{

VkImageType SelectVulkanImageType(const rhi::TextureDefinition& definition)
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

VkFormat GetVulkanFormat(rhi::EFragmentFormat format)
{
    switch (format)
    {
    case rhi::EFragmentFormat::RGBA8_UN_Float:      return VK_FORMAT_R8G8B8A8_UNORM;
    case rhi::EFragmentFormat::BGRA8_UN_Float:      return VK_FORMAT_B8G8R8A8_UNORM;

    case rhi::EFragmentFormat::RGB8_UN_Float:       return VK_FORMAT_R8G8B8_UNORM;
    case rhi::EFragmentFormat::BGR8_UN_Float:       return VK_FORMAT_B8G8R8_UNORM;

    case rhi::EFragmentFormat::RGBA32_S_Float:      return VK_FORMAT_R32G32B32A32_SFLOAT;
    case rhi::EFragmentFormat::D32_S_Float:         return VK_FORMAT_D32_SFLOAT;
    }

    SPT_CHECK_NO_ENTRY();
    return VK_FORMAT_UNDEFINED;
}

rhi::EFragmentFormat GetRHIFormat(VkFormat format)
{
    switch (format)
    {
    case VK_FORMAT_R8G8B8A8_UNORM:                  return rhi::EFragmentFormat::RGBA8_UN_Float;
    case VK_FORMAT_B8G8R8A8_UNORM:                  return rhi::EFragmentFormat::BGRA8_UN_Float;
                                                           
    case VK_FORMAT_R8G8B8_UNORM:                    return rhi::EFragmentFormat::RGB8_UN_Float;
    case VK_FORMAT_B8G8R8_UNORM:                    return rhi::EFragmentFormat::BGR8_UN_Float;
                                                           
    case VK_FORMAT_R32G32B32A32_SFLOAT:             return rhi::EFragmentFormat::RGBA32_S_Float;
    case VK_FORMAT_D32_SFLOAT:                      return rhi::EFragmentFormat::D32_S_Float;
    }

    SPT_CHECK_NO_ENTRY();
    return rhi::EFragmentFormat::RGBA8_UN_Float;
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

Flags32 GetRHITextureUsageFlags(VkImageUsageFlags usageFlags)
{
    Flags32 rhiFlags = 0;

    if ((usageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) != 0)
    {
        rhiFlags |= rhi::ETextureUsage::TransferSource;
    }
    if ((usageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT) != 0)
    {
        rhiFlags |= rhi::ETextureUsage::TransferDest;
    }
	if ((usageFlags & VK_IMAGE_USAGE_SAMPLED_BIT) != 0)
    {
        rhiFlags |= rhi::ETextureUsage::SampledTexture;
    }
	if ((usageFlags & VK_IMAGE_USAGE_STORAGE_BIT) != 0)
    {
        rhiFlags |= rhi::ETextureUsage::StorageTexture;
    }
	if ((usageFlags & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) != 0)
    {
        rhiFlags |= rhi::ETextureUsage::ColorRT;
    }
	if ((usageFlags & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0)
    {
        rhiFlags |= rhi::ETextureUsage::DepthSetncilRT;
    }
	if ((usageFlags & VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT) != 0)
    {
        rhiFlags |= rhi::ETextureUsage::TransientRT;
    }
	if ((usageFlags & VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT) != 0)
    {
        rhiFlags |= rhi::ETextureUsage::InputRT;
    }
	if ((usageFlags & VK_IMAGE_USAGE_SHADING_RATE_IMAGE_BIT_NV) != 0)
    {
        rhiFlags |= rhi::ETextureUsage::ShadingRateTexture;
    }
	if ((usageFlags & VK_IMAGE_USAGE_FRAGMENT_DENSITY_MAP_BIT_EXT) != 0)
    {
        rhiFlags |= rhi::ETextureUsage::FragmentDensityMap;
    }

    return rhiFlags;
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

VkImageViewType GetVulkanViewType(rhi::ETextureViewType viewType, const rhi::TextureSubresourceRange& range, const rhi::TextureDefinition& textureDef)
{
    if (viewType == rhi::ETextureViewType::Default)
    {
        if (textureDef.m_resolution.z() > 1)
        {
            return VK_IMAGE_VIEW_TYPE_3D;
        }
        else if (textureDef.m_resolution.y() > 1)
        {
            return range.m_arrayLayersNum > 1 ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
        }
        else
        {
            return range.m_arrayLayersNum > 1 ? VK_IMAGE_VIEW_TYPE_1D_ARRAY : VK_IMAGE_VIEW_TYPE_1D;
        }
    }
    else
    {
        return (range.m_arrayLayersNum / 6) > 1 ? VK_IMAGE_VIEW_TYPE_CUBE_ARRAY : VK_IMAGE_VIEW_TYPE_CUBE;
    }
}

VkComponentSwizzle GetVulkanComponentMapping(rhi::ETextureComponentMapping mapping)
{
    switch (mapping)
    {
    case rhi::ETextureComponentMapping::R:            return VK_COMPONENT_SWIZZLE_R;
    case rhi::ETextureComponentMapping::G:            return VK_COMPONENT_SWIZZLE_G;
    case rhi::ETextureComponentMapping::B:            return VK_COMPONENT_SWIZZLE_B;
    case rhi::ETextureComponentMapping::A:            return VK_COMPONENT_SWIZZLE_A;
    case rhi::ETextureComponentMapping::Zero:         return VK_COMPONENT_SWIZZLE_ZERO;
    case rhi::ETextureComponentMapping::One:          return VK_COMPONENT_SWIZZLE_ONE;
    }

    SPT_CHECK_NO_ENTRY();
    return VK_COMPONENT_SWIZZLE_ZERO;
}

VkImageAspectFlags GetVulkanAspect(Flags32 aspect)
{
    VkImageAspectFlags vulkanAspect = 0;

    if ((aspect & rhi::ETextureAspect::Color) != 0)
    {
        vulkanAspect |= VK_IMAGE_ASPECT_COLOR_BIT;
    }
	if ((aspect & rhi::ETextureAspect::Depth) != 0)
    {
        vulkanAspect |= VK_IMAGE_ASPECT_DEPTH_BIT;
    }
	if ((aspect & rhi::ETextureAspect::Stencil) != 0)
    {
        vulkanAspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
    }

    return vulkanAspect;
}

}

//////////////////////////////////////////////////////////////////////////////////////////////////
// RHITexture ====================================================================================

RHITexture::RHITexture()
    : m_imageHandle(VK_NULL_HANDLE)
    , m_allocation(VK_NULL_HANDLE)
{ }

void RHITexture::InitializeRHI(const rhi::TextureDefinition& definition, VkImage imageHandle)
{
    SPT_CHECK(!IsValid());

    m_imageHandle = imageHandle;
    m_allocation = VK_NULL_HANDLE;
    m_definition = definition;
}

void RHITexture::InitializeRHI(const rhi::TextureDefinition& definition, const rhi::RHIAllocationInfo& allocation)
{
	SPT_PROFILE_FUNCTION();

    SPT_CHECK(!IsValid());

	VkImageCreateInfo imageInfo{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
    imageInfo.flags             = 0;
    imageInfo.imageType         = priv::SelectVulkanImageType(definition);
    imageInfo.format            = priv::GetVulkanFormat(definition.m_format);
    imageInfo.extent            = { definition.m_resolution.x(), definition.m_resolution.y(), definition.m_resolution.z() };
    imageInfo.mipLevels         = definition.m_mipLevels;
    imageInfo.arrayLayers       = definition.m_arrayLayers;
    imageInfo.samples           = priv::GetVulkanSampleCountFlag(definition.m_samples);
    imageInfo.tiling            = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage             = priv::GetVulkanTextureUsageFlags(definition.m_usage);
    imageInfo.sharingMode       = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.initialLayout     = VK_IMAGE_LAYOUT_UNDEFINED;

	const VmaAllocationCreateInfo allocationInfo = VulkanRHI::GetMemoryManager().CreateAllocationInfo(allocation);

	SPT_VK_CHECK(vmaCreateImage(VulkanRHI::GetAllocatorHandle(), &imageInfo, &allocationInfo, &m_imageHandle, &m_allocation, nullptr));

    m_definition = definition;
}

void RHITexture::ReleaseRHI()
{
    SPT_PROFILE_FUNCTION();

    SPT_CHECK(!!IsValid());

    if (m_allocation)
    {
        vmaDestroyImage(VulkanRHI::GetAllocatorHandle(), m_imageHandle, m_allocation);
    }

    m_imageHandle   = VK_NULL_HANDLE;
    m_allocation    = VK_NULL_HANDLE;
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

VkImageUsageFlags RHITexture::GetVulkanTextureUsageFlags(Flags32 usageFlags)
{
    return priv::GetVulkanTextureUsageFlags(usageFlags);
}

VkFormat RHITexture::GetVulkanFormat(rhi::EFragmentFormat format)
{
    return priv::GetVulkanFormat(format);
}

Flags32 RHITexture::GetRHITextureUsageFlags(VkImageUsageFlags usageFlags)
{
    return priv::GetRHITextureUsageFlags(usageFlags);
}

rhi::EFragmentFormat RHITexture::GetRHIFormat(VkFormat format)
{
    return priv::GetRHIFormat(format);
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// RHITextureView ================================================================================

RHITextureView::RHITextureView()
    : m_viewHandle(VK_NULL_HANDLE)
    , m_texture(nullptr)
{ }

void RHITextureView::InitializeRHI(const RHITexture& texture, const rhi::TextureViewDefinition& viewDefinition)
{
    SPT_PROFILE_FUNCTION();

    const rhi::TextureDefinition& textureDef = texture.GetDefinition();

    VkImageViewCreateInfo viewInfo{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
    viewInfo.flags      = 0;
    viewInfo.image      = texture.GetHandle();
    viewInfo.viewType   = priv::GetVulkanViewType(viewDefinition.m_viewType, viewDefinition.m_subresourceRange, textureDef);
    viewInfo.format     = priv::GetVulkanFormat(textureDef.m_format);

    viewInfo.components.r = priv::GetVulkanComponentMapping(viewDefinition.m_componentMappings.r);
    viewInfo.components.g = priv::GetVulkanComponentMapping(viewDefinition.m_componentMappings.g);
    viewInfo.components.b = priv::GetVulkanComponentMapping(viewDefinition.m_componentMappings.b);
    viewInfo.components.a = priv::GetVulkanComponentMapping(viewDefinition.m_componentMappings.a);

    viewInfo.subresourceRange.aspectMask        = priv::GetVulkanAspect(viewDefinition.m_subresourceRange.m_aspect);
    viewInfo.subresourceRange.baseMipLevel      = viewDefinition.m_subresourceRange.m_baseMipLevel;
    viewInfo.subresourceRange.levelCount        = viewDefinition.m_subresourceRange.m_mipLevelsNum;
    viewInfo.subresourceRange.baseArrayLayer    = viewDefinition.m_subresourceRange.m_baseArrayLayer;
    viewInfo.subresourceRange.layerCount        = viewDefinition.m_subresourceRange.m_arrayLayersNum;

    SPT_VK_CHECK(vkCreateImageView(VulkanRHI::GetDeviceHandle(), &viewInfo, VulkanRHI::GetAllocationCallbacks(), &m_viewHandle));

    m_texture = &texture;
}

void RHITextureView::ReleaseRHI()
{
    SPT_PROFILE_FUNCTION();

    SPT_CHECK(!!m_viewHandle);

    vkDestroyImageView(VulkanRHI::GetDeviceHandle(), m_viewHandle, VulkanRHI::GetAllocationCallbacks());

    m_viewHandle = VK_NULL_HANDLE;
    m_texture = nullptr;
}

Bool RHITextureView::IsValid() const
{
    return !!m_viewHandle;
}

VkImageView RHITextureView::GetHandle() const
{
    return m_viewHandle;
}

const RHITexture* RHITextureView::GetTexture() const
{
    return m_texture;
}

}