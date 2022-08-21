#include "RHITexture.h"
#include "Vulkan/VulkanRHI.h"
#include "Vulkan/Memory/MemoryManager.h"
#include "Vulkan/LayoutsManager.h"


namespace spt::vulkan
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// Helpers =======================================================================================

namespace priv
{

VkImageType SelectVulkanImageType(const rhi::TextureDefinition& definition)
{
    if (definition.resolution.z() > 1)
    {
        return VK_IMAGE_TYPE_3D;
    }
    else if (definition.resolution.y() > 1)
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

    default:

        SPT_CHECK_NO_ENTRY();
        return VK_FORMAT_UNDEFINED;
    }
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

    default:

        SPT_CHECK_NO_ENTRY();
        return rhi::EFragmentFormat::RGBA8_UN_Float;
    }
}

VkImageUsageFlags GetVulkanTextureUsageFlags(rhi::ETextureUsage usageFlags)
{
    VkImageUsageFlags vulkanFlags = 0;

    if (lib::HasAnyFlag(usageFlags, rhi::ETextureUsage::TransferSource))
    {
        lib::AddFlag(vulkanFlags, VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    }
    if (lib::HasAnyFlag(usageFlags, rhi::ETextureUsage::TransferDest))
    {
        lib::AddFlag(vulkanFlags, VK_IMAGE_USAGE_TRANSFER_DST_BIT);
    }
	if (lib::HasAnyFlag(usageFlags, rhi::ETextureUsage::SampledTexture))
    {
        lib::AddFlag(vulkanFlags, VK_IMAGE_USAGE_SAMPLED_BIT);
    }
	if (lib::HasAnyFlag(usageFlags, rhi::ETextureUsage::StorageTexture))
    {
        lib::AddFlag(vulkanFlags, VK_IMAGE_USAGE_STORAGE_BIT);
    }
	if (lib::HasAnyFlag(usageFlags, rhi::ETextureUsage::ColorRT))
    {
        lib::AddFlag(vulkanFlags, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
    }
	if (lib::HasAnyFlag(usageFlags, rhi::ETextureUsage::DepthSetncilRT))
    {
        lib::AddFlag(vulkanFlags, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
    }
	if (lib::HasAnyFlag(usageFlags, rhi::ETextureUsage::TransientRT))
    {
        lib::AddFlag(vulkanFlags, VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT);
    }
	if (lib::HasAnyFlag(usageFlags, rhi::ETextureUsage::InputRT))
    {
        lib::AddFlag(vulkanFlags, VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT);
    }
	if (lib::HasAnyFlag(usageFlags, rhi::ETextureUsage::ShadingRateTexture))
    {
        lib::AddFlag(vulkanFlags, VK_IMAGE_USAGE_SHADING_RATE_IMAGE_BIT_NV);
    }
	if (lib::HasAnyFlag(usageFlags, rhi::ETextureUsage::FragmentDensityMap))
    {
        lib::AddFlag(vulkanFlags, VK_IMAGE_USAGE_FRAGMENT_DENSITY_MAP_BIT_EXT);
    }

    return vulkanFlags;
}

rhi::ETextureUsage GetRHITextureUsageFlags(VkImageUsageFlags usageFlags)
{
    rhi::ETextureUsage rhiFlags = rhi::ETextureUsage::None;

    if (lib::HasAnyFlag(usageFlags, VK_IMAGE_USAGE_TRANSFER_SRC_BIT))
    {
        lib::AddFlag(rhiFlags, rhi::ETextureUsage::TransferSource);
    }
    if (lib::HasAnyFlag(usageFlags, VK_IMAGE_USAGE_TRANSFER_DST_BIT))
    {
        lib::AddFlag(rhiFlags, rhi::ETextureUsage::TransferDest);
    }
	if (lib::HasAnyFlag(usageFlags, VK_IMAGE_USAGE_SAMPLED_BIT))
    {
        lib::AddFlag(rhiFlags, rhi::ETextureUsage::SampledTexture);
    }
	if (lib::HasAnyFlag(usageFlags, VK_IMAGE_USAGE_STORAGE_BIT))
    {
        lib::AddFlag(rhiFlags, rhi::ETextureUsage::StorageTexture);
    }
	if (lib::HasAnyFlag(usageFlags, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT))
    {
        lib::AddFlag(rhiFlags, rhi::ETextureUsage::ColorRT);
    }
	if (lib::HasAnyFlag(usageFlags, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT))
    {
        lib::AddFlag(rhiFlags, rhi::ETextureUsage::DepthSetncilRT);
    }
	if (lib::HasAnyFlag(usageFlags, VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT))
    {
        lib::AddFlag(rhiFlags, rhi::ETextureUsage::TransientRT);
    }
	if (lib::HasAnyFlag(usageFlags, VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT))
    {
        lib::AddFlag(rhiFlags, rhi::ETextureUsage::InputRT);
    }
	if (lib::HasAnyFlag(usageFlags, VK_IMAGE_USAGE_SHADING_RATE_IMAGE_BIT_NV))
    {
        lib::AddFlag(rhiFlags, rhi::ETextureUsage::ShadingRateTexture);
    }
	if (lib::HasAnyFlag(usageFlags, VK_IMAGE_USAGE_FRAGMENT_DENSITY_MAP_BIT_EXT))
    {
        lib::AddFlag(rhiFlags, rhi::ETextureUsage::FragmentDensityMap);
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
        if (textureDef.resolution.z() > 1)
        {
            return VK_IMAGE_VIEW_TYPE_3D;
        }
        else if (textureDef.resolution.y() > 1)
        {
            return range.arrayLayersNum > 1 ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
        }
        else
        {
            return range.arrayLayersNum > 1 ? VK_IMAGE_VIEW_TYPE_1D_ARRAY : VK_IMAGE_VIEW_TYPE_1D;
        }
    }
    else
    {
        return (range.arrayLayersNum / 6) > 1 ? VK_IMAGE_VIEW_TYPE_CUBE_ARRAY : VK_IMAGE_VIEW_TYPE_CUBE;
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

VkImageAspectFlags GetVulkanAspect(rhi::ETextureAspect aspect)
{
    VkImageAspectFlags vulkanAspect = 0;

    if (lib::HasAnyFlag(aspect, rhi::ETextureAspect::Color))
    {
        lib::AddFlag(vulkanAspect, VK_IMAGE_ASPECT_COLOR_BIT);
    }
	if (lib::HasAnyFlag(aspect, rhi::ETextureAspect::Depth))
    {
        lib::AddFlag(vulkanAspect, VK_IMAGE_ASPECT_DEPTH_BIT);
    }
	if (lib::HasAnyFlag(aspect, rhi::ETextureAspect::Stencil))
    {
        lib::AddFlag(vulkanAspect, VK_IMAGE_ASPECT_STENCIL_BIT);
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

    m_imageHandle   = imageHandle;
    m_allocation    = VK_NULL_HANDLE;
    m_definition    = definition;
    
    PostImageInitialized();
}

void RHITexture::InitializeRHI(const rhi::TextureDefinition& definition, const rhi::RHIAllocationInfo& allocation)
{
	SPT_PROFILE_FUNCTION();

    SPT_CHECK(!IsValid());

	VkImageCreateInfo imageInfo{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
    imageInfo.flags             = 0;
    imageInfo.imageType         = priv::SelectVulkanImageType(definition);
    imageInfo.format            = priv::GetVulkanFormat(definition.format);
    imageInfo.extent            = { definition.resolution.x(), definition.resolution.y(), definition.resolution.z() };
    imageInfo.mipLevels         = definition.mipLevels;
    imageInfo.arrayLayers       = definition.arrayLayers;
    imageInfo.samples           = priv::GetVulkanSampleCountFlag(definition.samples);
    imageInfo.tiling            = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage             = priv::GetVulkanTextureUsageFlags(definition.usage);
    imageInfo.sharingMode       = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.initialLayout     = VK_IMAGE_LAYOUT_UNDEFINED;

	const VmaAllocationCreateInfo allocationInfo = VulkanRHI::GetMemoryManager().CreateAllocationInfo(allocation);

	SPT_VK_CHECK(vmaCreateImage(VulkanRHI::GetAllocatorHandle(), &imageInfo, &allocationInfo, &m_imageHandle, &m_allocation, nullptr));

    m_definition = definition;

    PostImageInitialized();
}

void RHITexture::ReleaseRHI()
{
    SPT_PROFILE_FUNCTION();

    SPT_CHECK(!!IsValid());

    if (m_allocation)
    {
        vmaDestroyImage(VulkanRHI::GetAllocatorHandle(), m_imageHandle, m_allocation);
    }

    PreImageReleased();

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

VkImageUsageFlags RHITexture::GetVulkanTextureUsageFlags(rhi::ETextureUsage usageFlags)
{
    return priv::GetVulkanTextureUsageFlags(usageFlags);
}

VkFormat RHITexture::GetVulkanFormat(rhi::EFragmentFormat format)
{
    return priv::GetVulkanFormat(format);
}

rhi::ETextureUsage RHITexture::GetRHITextureUsageFlags(VkImageUsageFlags usageFlags)
{
    return priv::GetRHITextureUsageFlags(usageFlags);
}

rhi::EFragmentFormat RHITexture::GetRHIFormat(VkFormat format)
{
    return priv::GetRHIFormat(format);
}

void RHITexture::PostImageInitialized()
{
    VulkanRHI::GetLayoutsManager().RegisterImage(m_imageHandle, m_definition.mipLevels, m_definition.arrayLayers);
}

void RHITexture::PreImageReleased()
{

    VulkanRHI::GetLayoutsManager().UnregisterImage(m_imageHandle);
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
    viewInfo.viewType   = priv::GetVulkanViewType(viewDefinition.viewType, viewDefinition.subresourceRange, textureDef);
    viewInfo.format     = priv::GetVulkanFormat(textureDef.format);

    viewInfo.components.r = priv::GetVulkanComponentMapping(viewDefinition.componentMappings.r);
    viewInfo.components.g = priv::GetVulkanComponentMapping(viewDefinition.componentMappings.g);
    viewInfo.components.b = priv::GetVulkanComponentMapping(viewDefinition.componentMappings.b);
    viewInfo.components.a = priv::GetVulkanComponentMapping(viewDefinition.componentMappings.a);

    viewInfo.subresourceRange.aspectMask        = priv::GetVulkanAspect(viewDefinition.subresourceRange.aspect);
    viewInfo.subresourceRange.baseMipLevel      = viewDefinition.subresourceRange.baseMipLevel;
    viewInfo.subresourceRange.levelCount        = viewDefinition.subresourceRange.mipLevelsNum;
    viewInfo.subresourceRange.baseArrayLayer    = viewDefinition.subresourceRange.baseArrayLayer;
    viewInfo.subresourceRange.layerCount        = viewDefinition.subresourceRange.arrayLayersNum;

    SPT_VK_CHECK(vkCreateImageView(VulkanRHI::GetDeviceHandle(), &viewInfo, VulkanRHI::GetAllocationCallbacks(), &m_viewHandle));

    m_texture = &texture;
    m_subresourceRange = viewDefinition.subresourceRange;
}

void RHITextureView::ReleaseRHI()
{
    SPT_PROFILE_FUNCTION();

    SPT_CHECK(!!m_viewHandle);

    vkDestroyImageView(VulkanRHI::GetDeviceHandle(), m_viewHandle, VulkanRHI::GetAllocationCallbacks());

    m_viewHandle = VK_NULL_HANDLE;
    m_texture = nullptr;
    m_name.Reset();
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

const rhi::TextureSubresourceRange& RHITextureView::GetSubresourceRange() const
{
    return m_subresourceRange;
}

void RHITextureView::SetName(const lib::HashedString& name)
{
    m_name.Set(name, reinterpret_cast<Uint64>(m_viewHandle), VK_OBJECT_TYPE_IMAGE_VIEW);
}

const lib::HashedString& RHITextureView::GetName() const
{
    return m_name.Get();
}

}