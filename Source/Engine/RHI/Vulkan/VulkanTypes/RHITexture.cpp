#include "RHITexture.h"
#include "Vulkan/VulkanRHI.h"
#include "Vulkan/Memory/MemoryManager.h"
#include "Vulkan/LayoutsManager.h"
#include "Vulkan/VulkanRHIUtils.h"


namespace spt::vulkan
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// Helpers =======================================================================================

namespace priv
{

VkImageType SelectVulkanImageType(const rhi::TextureDefinition& definition)
{
    if (definition.resolution.AsVector().z() > 1)
    {
        return VK_IMAGE_TYPE_3D;
    }
    else if (definition.resolution.AsVector().y() > 1)
    {
        return VK_IMAGE_TYPE_2D;
    }
    else
    {
        return VK_IMAGE_TYPE_1D;
    }
}

rhi::EFragmentFormat GetRHIFormat(VkFormat format)
{
    switch (format)
    {
    case VK_FORMAT_R8_UNORM:                        return rhi::EFragmentFormat::R8_UN_Float;
    case VK_FORMAT_R16_UNORM:                       return rhi::EFragmentFormat::R16_UN_Float;
    case VK_FORMAT_R32_SFLOAT:                      return rhi::EFragmentFormat::R32_S_Float;
    
    case VK_FORMAT_R8G8_UNORM:                      return rhi::EFragmentFormat::RG8_UN_Float;
    case VK_FORMAT_R16G16_UNORM:                    return rhi::EFragmentFormat::RG16_UN_Float;
    case VK_FORMAT_R16G16_SNORM:                    return rhi::EFragmentFormat::RG16_SN_Float;
    case VK_FORMAT_R16G16_SFLOAT:                   return rhi::EFragmentFormat::RG16_S_Float;
    case VK_FORMAT_R32G32_SFLOAT:                   return rhi::EFragmentFormat::RG32_S_Float;
                                                           
    case VK_FORMAT_R8G8B8_UNORM:                    return rhi::EFragmentFormat::RGB8_UN_Float;
    case VK_FORMAT_B8G8R8_UNORM:                    return rhi::EFragmentFormat::BGR8_UN_Float;
    case VK_FORMAT_R16G16B16_UNORM:                 return rhi::EFragmentFormat::RGB16_UN_Float;
    case VK_FORMAT_R32G32B32_SFLOAT:                return rhi::EFragmentFormat::RGB32_S_Float;

    case VK_FORMAT_B10G11R11_UFLOAT_PACK32:         return rhi::EFragmentFormat::B10G11R11_U_Float;
    case VK_FORMAT_A2B10G10R10_UNORM_PACK32:        return rhi::EFragmentFormat::A2B10G10R10_UN_Float;

    case VK_FORMAT_R8G8B8A8_UNORM:                  return rhi::EFragmentFormat::RGBA8_UN_Float;
    case VK_FORMAT_B8G8R8A8_UNORM:                  return rhi::EFragmentFormat::BGRA8_UN_Float;
    case VK_FORMAT_R16G16B16A16_UNORM:              return rhi::EFragmentFormat::RGBA16_UN_Float;
    case VK_FORMAT_R16G16B16A16_SFLOAT:             return rhi::EFragmentFormat::RGBA16_S_Float;

                                                           
    case VK_FORMAT_R32G32B32A32_SFLOAT:             return rhi::EFragmentFormat::RGBA32_S_Float;
    
    case VK_FORMAT_D16_UNORM:                       return rhi::EFragmentFormat::D16_UN_Float;
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

static Bool IsArrayView(const rhi::TextureSubresourceRange& range, const rhi::TextureDefinition& textureDef)
{
    return range.arrayLayersNum == rhi::constants::allRemainingArrayLayers
        ? textureDef.arrayLayers - range.baseArrayLayer > 1
        : range.arrayLayersNum > 1;
}

static Bool IsCubeArrayView(const rhi::TextureSubresourceRange& range, const rhi::TextureDefinition& textureDef)
{
    return range.arrayLayersNum == rhi::constants::allRemainingArrayLayers
        ? (textureDef.arrayLayers - range.baseArrayLayer) / 6 > 1
        : (range.arrayLayersNum / 6) > 1;
}

VkImageViewType GetVulkanViewType(rhi::ETextureViewType viewType, const rhi::TextureSubresourceRange& range, const rhi::TextureDefinition& textureDef)
{
    if (viewType == rhi::ETextureViewType::Default)
    {
        if (textureDef.resolution.AsVector().z() > 1)
        {
            return VK_IMAGE_VIEW_TYPE_3D;
        }
        else if (textureDef.resolution.AsVector().y() > 1)
        {
            return IsArrayView(range, textureDef) ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
        }
        else
        {
            return IsArrayView(range, textureDef) ? VK_IMAGE_VIEW_TYPE_1D_ARRAY : VK_IMAGE_VIEW_TYPE_1D;
        }
    }
    else
    {
        return IsCubeArrayView(range, textureDef) ? VK_IMAGE_VIEW_TYPE_CUBE_ARRAY : VK_IMAGE_VIEW_TYPE_CUBE;
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

void RHITexture::InitializeRHI(const rhi::TextureDefinition& definition, VkImage imageHandle, rhi::EMemoryUsage memoryUsage)
{
    SPT_CHECK(!IsValid());

    m_imageHandle   = imageHandle;
    m_allocation    = VK_NULL_HANDLE;
    m_definition    = definition;

    m_allocationInfo.memoryUsage = memoryUsage;
    m_allocationInfo.allocationFlags = rhi::EAllocationFlags::Unknown;
    
    PostImageInitialized();
}

void RHITexture::InitializeRHI(const rhi::TextureDefinition& definition, const rhi::RHIAllocationInfo& allocationDef)
{
	SPT_PROFILER_FUNCTION();

    SPT_CHECK(!IsValid());

    const math::Vector3u resolution = definition.resolution.AsVector();

    SPT_CHECK(resolution.x() > 0);
    SPT_CHECK(resolution.y() > 0);
    SPT_CHECK(resolution.z() > 0);

	VkImageCreateInfo imageInfo{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
    imageInfo.flags             = 0;
    imageInfo.imageType         = priv::SelectVulkanImageType(definition);
    imageInfo.format            = RHIToVulkan::GetVulkanFormat(definition.format);
    imageInfo.extent            = { resolution.x(), resolution.y(), resolution.z() };
    imageInfo.mipLevels         = definition.mipLevels;
    imageInfo.arrayLayers       = definition.arrayLayers;
    imageInfo.samples           = priv::GetVulkanSampleCountFlag(definition.samples);
    imageInfo.tiling            = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage             = priv::GetVulkanTextureUsageFlags(definition.usage);
    imageInfo.sharingMode       = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.initialLayout     = VK_IMAGE_LAYOUT_UNDEFINED;

	const VmaAllocationCreateInfo allocationInfo = VulkanRHI::GetMemoryManager().CreateAllocationInfo(allocationDef);

	SPT_VK_CHECK(vmaCreateImage(VulkanRHI::GetAllocatorHandle(), &imageInfo, &allocationInfo, &m_imageHandle, &m_allocation, nullptr));

    m_definition = definition;
    m_allocationInfo = allocationDef;

    PostImageInitialized();
}

void RHITexture::ReleaseRHI()
{
    SPT_PROFILER_FUNCTION();

    SPT_CHECK(!!IsValid());

    PreImageReleased();

    if (m_allocation)
    {
		m_name.Reset(reinterpret_cast<Uint64>(m_imageHandle), VK_OBJECT_TYPE_IMAGE);
        vmaDestroyImage(VulkanRHI::GetAllocatorHandle(), m_imageHandle, m_allocation);
    }

    m_imageHandle   = VK_NULL_HANDLE;
    m_allocation    = VK_NULL_HANDLE;

    m_definition        = rhi::TextureDefinition();
    m_allocationInfo    = rhi::RHIAllocationInfo();
}

Bool RHITexture::IsValid() const
{
    return !!m_imageHandle;
}

const rhi::TextureDefinition& RHITexture::GetDefinition() const
{
    return m_definition;
}

const math::Vector3u& RHITexture::GetResolution() const
{
    return GetDefinition().resolution.AsVector();
}

math::Vector3u RHITexture::GetMipResolution(Uint32 mipLevel) const
{
    SPT_CHECK(mipLevel < static_cast<Uint32>(GetDefinition().mipLevels));

    math::Vector3u resolution = GetResolution();
    resolution.x() = std::max<Uint32>(resolution.x() >> mipLevel, 1);
    resolution.y() = std::max<Uint32>(resolution.y() >> mipLevel, 1);
    resolution.z() = std::max<Uint32>(resolution.z() >> mipLevel, 1);

    return resolution;
}

const rhi::RHIAllocationInfo& RHITexture::GetAllocationInfo() const
{
    return m_allocationInfo;
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
    return RHIToVulkan::GetVulkanFormat(format);
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
    SPT_PROFILER_FUNCTION();

    const rhi::TextureDefinition& textureDef = texture.GetDefinition();

    VkImageViewCreateInfo viewInfo{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
    viewInfo.flags      = 0;
    viewInfo.image      = texture.GetHandle();
    viewInfo.viewType   = priv::GetVulkanViewType(viewDefinition.viewType, viewDefinition.subresourceRange, textureDef);
    viewInfo.format     = RHIToVulkan::GetVulkanFormat(textureDef.format);

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
    SPT_PROFILER_FUNCTION();

    SPT_CHECK(!!m_viewHandle);

	m_name.Reset(reinterpret_cast<Uint64>(m_viewHandle), VK_OBJECT_TYPE_IMAGE_VIEW);

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