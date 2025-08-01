#include "RHITexture.h"
#include "Vulkan/VulkanRHI.h"
#include "Vulkan/Memory/VulkanMemoryManager.h"
#include "Vulkan/LayoutsManager.h"
#include "Vulkan/VulkanRHIUtils.h"
#include "Vulkan/Memory/VulkanMemoryTypes.h"
#include "Vulkan/Device/LogicalDevice.h"
#include "MathUtils.h"
#include "RHIGPUMemoryPool.h"
#include "Utility/Templates/Overload.h"


namespace spt::vulkan
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// Helpers =======================================================================================

namespace priv
{

static rhi::ETextureType SelectValidTextureType(const rhi::TextureDefinition& definition)
{
	switch (definition.type)
	{
	case rhi::ETextureType::Texture1D:
	case rhi::ETextureType::Texture2D:
	case rhi::ETextureType::Texture3D:
		return definition.type;

	case rhi::ETextureType::Auto:
		{
			const math::Vector3u resolution = definition.resolution.AsVector();
			if (resolution.z() > 1u)
			{
				return rhi::ETextureType::Texture3D;
			}
			else if (resolution.y() > 1u)
			{
				return rhi::ETextureType::Texture2D;
			}
			else
			{
				return rhi::ETextureType::Texture1D;
			}
		}
		break;
	default:

		SPT_CHECK_NO_ENTRY();
	}

	return rhi::ETextureType::Auto;
}

static VkImageType GetVulkanImageType(rhi::ETextureType type)
{
	switch (type)
	{
	case rhi::ETextureType::Texture1D:  return VK_IMAGE_TYPE_1D;
	case rhi::ETextureType::Texture2D:  return VK_IMAGE_TYPE_2D;
	case rhi::ETextureType::Texture3D:  return VK_IMAGE_TYPE_3D;
	
	default:

	SPT_CHECK_NO_ENTRY();
	break;
	}

	return VK_IMAGE_TYPE_MAX_ENUM;
}

static rhi::EFragmentFormat GetRHIFormat(VkFormat format)
{
	switch (format)
	{
	case VK_FORMAT_R8_UNORM:                        return rhi::EFragmentFormat::R8_UN_Float;
	case VK_FORMAT_R8_UINT:                         return rhi::EFragmentFormat::R8_U_Int;
	case VK_FORMAT_R16_UINT:                        return rhi::EFragmentFormat::R16_U_Int;
	case VK_FORMAT_R16_UNORM:                       return rhi::EFragmentFormat::R16_UN_Float;
	case VK_FORMAT_R16_SFLOAT:                      return rhi::EFragmentFormat::R16_S_Float;
	case VK_FORMAT_R32_SFLOAT:                      return rhi::EFragmentFormat::R32_S_Float;
	case VK_FORMAT_R32_UINT:                        return rhi::EFragmentFormat::R32_U_Int;
	
	case VK_FORMAT_R8G8_UNORM:                      return rhi::EFragmentFormat::RG8_UN_Float;
	case VK_FORMAT_R16G16_UNORM:                    return rhi::EFragmentFormat::RG16_UN_Float;
	case VK_FORMAT_R16G16_UINT:                     return rhi::EFragmentFormat::RG16_U_Int;
	case VK_FORMAT_R16G16_SNORM:                    return rhi::EFragmentFormat::RG16_SN_Float;
	case VK_FORMAT_R16G16_SFLOAT:                   return rhi::EFragmentFormat::RG16_S_Float;
	case VK_FORMAT_R32G32_SFLOAT:                   return rhi::EFragmentFormat::RG32_S_Float;

	case VK_FORMAT_R8G8B8_UNORM:                    return rhi::EFragmentFormat::RGB8_UN_Float;
	case VK_FORMAT_B8G8R8_UNORM:                    return rhi::EFragmentFormat::BGR8_UN_Float;
	case VK_FORMAT_R16G16B16_UNORM:                 return rhi::EFragmentFormat::RGB16_UN_Float;
	case VK_FORMAT_R32G32B32_SFLOAT:                return rhi::EFragmentFormat::RGB32_S_Float;

	case VK_FORMAT_A2R10G10B10_UNORM_PACK32:        return rhi::EFragmentFormat::RGB10A2_UN_Float;
	case VK_FORMAT_B10G11R11_UFLOAT_PACK32:         return rhi::EFragmentFormat::B10G11R11_U_Float;
	case VK_FORMAT_A2B10G10R10_UNORM_PACK32:        return rhi::EFragmentFormat::A2B10G10R10_UN_Float;
	case VK_FORMAT_E5B9G9R9_UFLOAT_PACK32:          return rhi::EFragmentFormat::RGB9E5_Float;

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

static VkImageUsageFlags GetVulkanTextureUsageFlags(rhi::ETextureUsage usageFlags)
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

static rhi::ETextureUsage GetRHITextureUsageFlags(VkImageUsageFlags usageFlags)
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

static VkSampleCountFlagBits GetVulkanSampleCountFlag(Uint32 samples)
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

static VkImageViewType GetVulkanViewType(rhi::ETextureViewType viewType, const rhi::TextureSubresourceRange& range, const RHITexture& texture)
{
	SPT_CHECK(texture.GetType() != rhi::ETextureType::Auto);
	const rhi::TextureDefinition& textureDef = texture.GetDefinition();

	if (viewType == rhi::ETextureViewType::Default)
	{
		if (texture.GetType() == rhi::ETextureType::Texture3D)
		{
			return VK_IMAGE_VIEW_TYPE_3D;
		}
		else if (texture.GetType() == rhi::ETextureType::Texture2D)
		{
			return IsArrayView(range, textureDef) ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
		}
		else
		{
			SPT_CHECK(texture.GetType() == rhi::ETextureType::Texture1D);
			return IsArrayView(range, textureDef) ? VK_IMAGE_VIEW_TYPE_1D_ARRAY : VK_IMAGE_VIEW_TYPE_1D;
		}
	}
	else
	{
		return IsCubeArrayView(range, textureDef) ? VK_IMAGE_VIEW_TYPE_CUBE_ARRAY : VK_IMAGE_VIEW_TYPE_CUBE;
	}
}

static VkComponentSwizzle GetVulkanComponentMapping(rhi::ETextureComponentMapping mapping)
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

static VkImageAspectFlags GetVulkanAspect(rhi::ETextureAspect aspect)
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
// RHITextureReleaseTicket =======================================================================

void RHITextureReleaseTicket::ExecuteReleaseRHI()
{
	if (handle.IsValid())
	{
		vkDestroyImage(VulkanRHI::GetDeviceHandle(), handle.GetValue(), VulkanRHI::GetAllocationCallbacks());
		handle.Reset();
	}

	if (allocation.IsValid())
	{
		vmaFreeMemory(VulkanRHI::GetAllocatorHandle(), allocation.GetValue());
		allocation.Reset();
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// RHITexture ====================================================================================

RHITexture::RHITexture()
	: m_imageHandle(VK_NULL_HANDLE)
	, m_type(rhi::ETextureType::Auto)
{ }

void RHITexture::InitializeRHI(const rhi::TextureDefinition& definition, VkImage imageHandle, rhi::EMemoryUsage memoryUsage)
{
	SPT_CHECK(!IsValid());

	m_imageHandle      = imageHandle;
	m_definition       = definition;
	m_allocationHandle = rhi::RHIExternalAllocation();

	m_type = priv::SelectValidTextureType(definition);
	SPT_CHECK(m_type != rhi::ETextureType::Auto);

	m_allocationInfo.memoryUsage = memoryUsage;
	m_allocationInfo.allocationFlags = rhi::EAllocationFlags::Unknown;
	
	PostImageInitialized();
}

void RHITexture::InitializeRHI(const rhi::TextureDefinition& definition, const rhi::RHIResourceAllocationDefinition& allocationDef)
{
	SPT_CHECK(!IsValid());

	const math::Vector3u resolution = definition.resolution.AsVector();

	SPT_CHECK(resolution.x() > 0);
	SPT_CHECK(resolution.y() > 0);
	SPT_CHECK(resolution.z() > 0);

	m_type = priv::SelectValidTextureType(definition);
	SPT_CHECK(m_type != rhi::ETextureType::Auto);

	VkImageCreateInfo imageInfo{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
	imageInfo.flags             = 0;
	imageInfo.imageType         = priv::GetVulkanImageType(m_type);
	imageInfo.format            = RHIToVulkan::GetVulkanFormat(definition.format);
	imageInfo.extent            = { resolution.x(), resolution.y(), resolution.z() };
	imageInfo.mipLevels         = definition.mipLevels;
	imageInfo.arrayLayers       = definition.arrayLayers;
	imageInfo.samples           = priv::GetVulkanSampleCountFlag(definition.samples);
	imageInfo.tiling            = RHIToVulkan::GetImageTiling(definition.tiling);
	imageInfo.usage             = priv::GetVulkanTextureUsageFlags(definition.usage);
	imageInfo.sharingMode       = VK_SHARING_MODE_EXCLUSIVE;
	imageInfo.initialLayout     = VK_IMAGE_LAYOUT_UNDEFINED;

	SPT_VK_CHECK(vkCreateImage(VulkanRHI::GetDeviceHandle(), &imageInfo, VulkanRHI::GetAllocationCallbacks(), &m_imageHandle));

	BindMemory(allocationDef);

	m_definition = definition;

	PostImageInitialized();
}

void RHITexture::ReleaseRHI()
{
	RHITextureReleaseTicket releaseTicket = DeferredReleaseRHI();
	releaseTicket.ExecuteReleaseRHI();
}

RHITextureReleaseTicket RHITexture::DeferredReleaseRHI()
{
	SPT_CHECK(IsValid());
	SPT_CHECK_MSG(!std::holds_alternative<rhi::RHIPlacedAllocation>(m_allocationHandle), "Placed allocations must be released manually before releasing resource!");

	PreImageReleased();

	RHITextureReleaseTicket releaseTicket;

	if (!std::holds_alternative<rhi::RHIExternalAllocation>(m_allocationHandle))
	{
		releaseTicket.handle = m_imageHandle;

#if SPT_RHI_DEBUG
		releaseTicket.name = GetName();
#endif SPT_RHI_DEBUG

		m_name.Reset(reinterpret_cast<Uint64>(m_imageHandle), VK_OBJECT_TYPE_IMAGE);

		if (std::holds_alternative<rhi::RHICommittedAllocation>(m_allocationHandle))
		{
			releaseTicket.allocation = memory_utils::GetVMAAllocation(std::get<rhi::RHICommittedAllocation>(m_allocationHandle));
			SPT_CHECK(!!releaseTicket.allocation.IsValid());
		}
	}

	m_imageHandle = VK_NULL_HANDLE;

	m_definition       = rhi::TextureDefinition();
	m_allocationInfo   = rhi::RHIAllocationInfo();
	m_allocationHandle = rhi::RHINullAllocation();

	SPT_CHECK(!IsValid());

	return releaseTicket;
}

Bool RHITexture::IsValid() const
{
	return !!m_imageHandle;
}

Bool RHITexture::HasBoundMemory() const
{
	return !std::holds_alternative<rhi::RHINullAllocation>(m_allocationHandle);
}

Bool RHITexture::IsPlacedAllocation() const
{
	return std::holds_alternative<rhi::RHIPlacedAllocation>(m_allocationHandle);
}

Bool RHITexture::IsCommittedAllocation() const
{
	return std::holds_alternative<rhi::RHICommittedAllocation>(m_allocationHandle);
}

rhi::RHIMemoryRequirements RHITexture::GetMemoryRequirements() const
{
	SPT_CHECK(IsValid());

	rhi::RHIMemoryRequirements memoryRequirements;

	VkMemoryRequirements vkMemoryRequirements;
	vkGetImageMemoryRequirements(VulkanRHI::GetDeviceHandle(), m_imageHandle, OUT &vkMemoryRequirements);

	memoryRequirements.size      = vkMemoryRequirements.size;
	memoryRequirements.alignment = vkMemoryRequirements.alignment;

	return memoryRequirements;
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

	return math::Utils::ComputeMipResolution(GetResolution(), mipLevel);
}

rhi::EFragmentFormat RHITexture::GetFormat() const
{
	return GetDefinition().format;
}

rhi::ETextureType RHITexture::GetType() const
{
	return m_type;
}

const rhi::RHIAllocationInfo& RHITexture::GetAllocationInfo() const
{
	SPT_CHECK(HasBoundMemory());
	return m_allocationInfo;
}

Uint64 RHITexture::GetFragmentSize() const
{
	SPT_CHECK(IsValid());
	return rhi::GetFragmentSize(GetFormat());
}

Uint64 RHITexture::GetMipSize(Uint32 mipIdx) const
{
	SPT_CHECK(IsValid());

	const math::Vector3u mipResolution = GetMipResolution(mipIdx);

	return GetFragmentSize() * mipResolution.x() * mipResolution.y() * mipResolution.z();
}

VkImage RHITexture::GetHandle() const
{
	return m_imageHandle;
}

Byte* RHITexture::MapPtr() const
{
	SPT_CHECK(IsValid());
	SPT_CHECK(HasBoundMemory());

	const rhi::RHIAllocationInfo allocationInfo = GetAllocationInfo();

	const VmaAllocation allocation = memory_utils::GetVMAAllocation(m_allocationHandle);

	void* mappedPtr = nullptr;
	const VkResult result = vmaMapMemory(VulkanRHI::GetAllocatorHandle(), allocation, OUT &mappedPtr);

	return result == VK_SUCCESS ? reinterpret_cast<Byte*>(mappedPtr) : nullptr;
}

void RHITexture::Unmap() const
{
	SPT_CHECK(IsValid());
	SPT_CHECK(HasBoundMemory());

	const VmaAllocation allocation = memory_utils::GetVMAAllocation(m_allocationHandle);

	vmaUnmapMemory(VulkanRHI::GetAllocatorHandle(), allocation);
}

void RHITexture::SetName(const lib::HashedString& name)
{
	m_name.Set(name, reinterpret_cast<Uint64>(m_imageHandle), VK_OBJECT_TYPE_IMAGE);

#if SPT_RHI_DEBUG
	if (std::holds_alternative<rhi::RHICommittedAllocation>(m_allocationHandle))
	{
		const VmaAllocation allocation = memory_utils::GetVMAAllocation(std::get<rhi::RHICommittedAllocation>(m_allocationHandle));
		vmaSetAllocationName(VulkanRHI::GetAllocatorHandle(), allocation, name.GetData());
	}
#endif // SPT_RHI_DEBUG
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
	// In case of images not owned by rhi, it's user responsibility to unregister image
	if (!std::holds_alternative<rhi::RHIExternalAllocation>(m_allocationHandle))
	{
		VulkanRHI::GetLayoutsManager().UnregisterImage(m_imageHandle);
	}
}

Bool RHITexture::BindMemory(const rhi::RHIResourceAllocationDefinition& allocationDefinition)
{
	SPT_CHECK(IsValid());
	SPT_CHECK(!HasBoundMemory());

	m_allocationHandle = std::visit(lib::Overload
									{
										[&](const rhi::RHINullAllocationDefinition& nullAllocation) -> rhi::RHIResourceAllocationHandle
										{
											return rhi::RHINullAllocation{};
										},
										[this](const rhi::RHIPlacedAllocationDefinition& placedAllocation) -> rhi::RHIResourceAllocationHandle
										{
											return DoPlacedAllocation(placedAllocation);
										},
										[&](const rhi::RHICommittedAllocationDefinition& committedAllocation) -> rhi::RHIResourceAllocationHandle
										{
											return DoCommittedAllocation(committedAllocation);
										}
									},
									allocationDefinition);

	const Bool success = HasBoundMemory();

	if (success)
	{
		const std::optional<rhi::RHIAllocationInfo> allocationInfo = memory_utils::GetAllocationInfo(allocationDefinition);
		SPT_CHECK(!!allocationInfo);
		m_allocationInfo = *allocationInfo;
	}

	return HasBoundMemory();
}

rhi::RHIResourceAllocationHandle RHITexture::ReleasePlacedAllocation()
{
	SPT_CHECK(IsValid());
	SPT_CHECK(std::holds_alternative<rhi::RHIPlacedAllocation>(m_allocationHandle));

	const rhi::RHIPlacedAllocation allocation = std::get<rhi::RHIPlacedAllocation>(m_allocationHandle);
	
	m_allocationHandle = rhi::RHINullAllocation{};

	return allocation;
}

rhi::RHIResourceAllocationHandle RHITexture::DoPlacedAllocation(const rhi::RHIPlacedAllocationDefinition& placedAllocationDef)
{
	SPT_CHECK(!!placedAllocationDef.pool);
	SPT_CHECK(placedAllocationDef.pool->IsValid());

	VkMemoryRequirements memoryRequirements{};
	vkGetImageMemoryRequirements(VulkanRHI::GetDeviceHandle(), m_imageHandle, OUT &memoryRequirements);

	rhi::VirtualAllocationDefinition suballocationDefinition{};
	suballocationDefinition.size      = memoryRequirements.size;
	suballocationDefinition.alignment = memoryRequirements.alignment;
	suballocationDefinition.flags     = placedAllocationDef.flags;

	const rhi::RHIVirtualAllocation suballocation = placedAllocationDef.pool->Allocate(suballocationDefinition);
	if (!suballocation.IsValid())
	{
		return rhi::RHINullAllocation{};
	}

	const VmaAllocation poolMemoryAllocation = placedAllocationDef.pool->GetAllocation();

	vmaBindImageMemory2(VulkanRHI::GetAllocatorHandle(), poolMemoryAllocation, suballocation.GetOffset(), m_imageHandle, nullptr);

	return rhi::RHIPlacedAllocation(rhi::RHICommittedAllocation(reinterpret_cast<Uint64>(poolMemoryAllocation)), suballocation);
}

rhi::RHIResourceAllocationHandle RHITexture::DoCommittedAllocation(const rhi::RHICommittedAllocationDefinition& committedAllocation)
{
	VmaAllocation allocation = VK_NULL_HANDLE;

	VmaAllocationCreateInfo allocationInfo{};
	allocationInfo.flags = memory_utils::GetVMAAllocationFlags(committedAllocation.allocationInfo.allocationFlags);
	allocationInfo.usage = memory_utils::GetVMAMemoryUsage(committedAllocation.allocationInfo.memoryUsage);
	SPT_VK_CHECK(vmaAllocateMemoryForImage(VulkanRHI::GetAllocatorHandle(), m_imageHandle, &allocationInfo, OUT &allocation, nullptr));

	SPT_CHECK(allocation != VK_NULL_HANDLE);

	vmaBindImageMemory2(VulkanRHI::GetAllocatorHandle(), allocation, 0, m_imageHandle, nullptr);

#if SPT_RHI_DEBUG
	if (m_name.HasName())
	{
		vmaSetAllocationName(VulkanRHI::GetAllocatorHandle(), allocation, m_name.Get().GetData());
	}
#endif // SPT_RHI_DEBUG

	return rhi::RHICommittedAllocation(reinterpret_cast<Uint64>(allocation));
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// RHITextureMemoryOwner =========================================================================

Bool RHITextureMemoryOwner::BindMemory(RHITexture& texture, const rhi::RHIResourceAllocationDefinition& allocationDefinition)
{
	return texture.BindMemory(allocationDefinition);
}

rhi::RHIResourceAllocationHandle RHITextureMemoryOwner::ReleasePlacedAllocation(RHITexture& texture)
{
	return texture.ReleasePlacedAllocation();
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// RHITextureViewReleaseTicket ===================================================================

void RHITextureViewReleaseTicket::ExecuteReleaseRHI()
{
	if (handle.IsValid())
	{
		vkDestroyImageView(VulkanRHI::GetDeviceHandle(), handle.GetValue(), VulkanRHI::GetAllocationCallbacks());
		handle.Reset();
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// RHITextureView ================================================================================

RHITextureView::RHITextureView()
	: m_viewHandle(VK_NULL_HANDLE)
	, m_texture(nullptr)
{ }

void RHITextureView::InitializeRHI(const RHITexture& texture, const rhi::TextureViewDefinition& viewDefinition)
{
	SPT_CHECK(!IsValid());
	SPT_CHECK(texture.IsValid());
	SPT_CHECK_MSG(!lib::HasAnyFlag(viewDefinition.subresourceRange.aspect, rhi::ETextureAspect::Auto)
			  || viewDefinition.subresourceRange.aspect == rhi::ETextureAspect::Auto, "Auto Aspect cannot be used with other flags");

	const rhi::TextureDefinition& textureDef = texture.GetDefinition();

	m_texture = &texture;
	m_subresourceRange = viewDefinition.subresourceRange;
	if (m_subresourceRange.aspect == rhi::ETextureAspect::Auto)
	{
		m_subresourceRange.aspect = rhi::GetFullAspectForFormat(textureDef.format);
	}

	VkImageViewCreateInfo viewInfo{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
	viewInfo.flags      = 0;
	viewInfo.image      = texture.GetHandle();
	viewInfo.viewType   = priv::GetVulkanViewType(viewDefinition.viewType, m_subresourceRange, texture);
	viewInfo.format     = RHIToVulkan::GetVulkanFormat(textureDef.format);

	viewInfo.components.r = priv::GetVulkanComponentMapping(viewDefinition.componentMappings.r);
	viewInfo.components.g = priv::GetVulkanComponentMapping(viewDefinition.componentMappings.g);
	viewInfo.components.b = priv::GetVulkanComponentMapping(viewDefinition.componentMappings.b);
	viewInfo.components.a = priv::GetVulkanComponentMapping(viewDefinition.componentMappings.a);

	viewInfo.subresourceRange.aspectMask        = priv::GetVulkanAspect(m_subresourceRange.aspect);
	viewInfo.subresourceRange.baseMipLevel      = m_subresourceRange.baseMipLevel;
	viewInfo.subresourceRange.levelCount        = m_subresourceRange.mipLevelsNum;
	viewInfo.subresourceRange.baseArrayLayer    = m_subresourceRange.baseArrayLayer;
	viewInfo.subresourceRange.layerCount        = m_subresourceRange.arrayLayersNum;

	SPT_VK_CHECK(vkCreateImageView(VulkanRHI::GetDeviceHandle(), &viewInfo, VulkanRHI::GetAllocationCallbacks(), &m_viewHandle));

	SPT_CHECK(IsValid());
}

void RHITextureView::ReleaseRHI()
{
	RHITextureViewReleaseTicket releaseTicket = DeferredReleaseRHI();
	releaseTicket.ExecuteReleaseRHI();
}

RHITextureViewReleaseTicket RHITextureView::DeferredReleaseRHI()
{
	SPT_CHECK(IsValid());

	RHITextureViewReleaseTicket releaseTicket;
	releaseTicket.handle = m_viewHandle;

#if SPT_RHI_DEBUG
	releaseTicket.name = GetName();
#endif // SPT_RHI_DEBUG

	m_name.Reset(reinterpret_cast<Uint64>(m_viewHandle), VK_OBJECT_TYPE_IMAGE_VIEW);

	m_viewHandle = VK_NULL_HANDLE;
	m_texture = nullptr;

	SPT_CHECK(!IsValid());

	return releaseTicket;
}

Bool RHITextureView::IsValid() const
{
	return !!m_viewHandle;
}

VkImageView RHITextureView::GetHandle() const
{
	return m_viewHandle;
}

void RHITextureView::CopyUAVDescriptor(Byte* dst) const
{
	SPT_CHECK(IsValid());
	SPT_CHECK(lib::HasAnyFlag(GetTexture()->GetDefinition().usage, rhi::ETextureUsage::StorageTexture));

	VkDescriptorImageInfo imageInfo{};
	imageInfo.imageView = GetHandle();

	VkDescriptorGetInfoEXT info{ VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT };
	info.type               = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	info.data.pStorageImage = &imageInfo;

	const LogicalDevice& device = VulkanRHI::GetLogicalDevice();

	vkGetDescriptorEXT(device.GetHandle(), &info, device.GetDescriptorProps().StrideFor(rhi::EDescriptorType::StorageTexture), dst);
}

void RHITextureView::CopySRVDescriptor(Byte* dst) const
{
	SPT_CHECK(IsValid());
	SPT_CHECK(lib::HasAnyFlag(GetTexture()->GetDefinition().usage, rhi::ETextureUsage::SampledTexture));

	VkDescriptorImageInfo imageInfo{};
	imageInfo.imageView = GetHandle();

	VkDescriptorGetInfoEXT info{ VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT };
	info.type               = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
	info.data.pStorageImage = &imageInfo;

	const LogicalDevice& device = VulkanRHI::GetLogicalDevice();

	vkGetDescriptorEXT(device.GetHandle(), &info, device.GetDescriptorProps().StrideFor(rhi::EDescriptorType::SampledTexture), dst);
}

const RHITexture* RHITextureView::GetTexture() const
{
	return m_texture;
}

math::Vector3u RHITextureView::GetResolution() const
{
	SPT_CHECK(m_texture);
	return m_texture->GetMipResolution(GetSubresourceRange().baseMipLevel);
}

math::Vector2u RHITextureView::GetResolution2D() const
{
	return GetResolution().head<2>();
}

rhi::EFragmentFormat RHITextureView::GetFormat() const
{
	SPT_CHECK(m_texture);
	return m_texture->GetFormat();
}

rhi::ETextureAspect RHITextureView::GetAspect() const
{
	SPT_CHECK(m_texture);
	return m_subresourceRange.aspect;
}

const rhi::TextureSubresourceRange& RHITextureView::GetSubresourceRange() const
{
	return m_subresourceRange;
}

Uint32 RHITextureView::GetMipLevelsNum() const
{
	SPT_CHECK(m_texture);
	return m_subresourceRange.mipLevelsNum == rhi::constants::allRemainingMips
		? m_texture->GetDefinition().mipLevels - m_subresourceRange.baseMipLevel
		: m_subresourceRange.mipLevelsNum;
}

Uint32 RHITextureView::GetArrayLevelsNum() const
{
	SPT_CHECK(m_texture);
	return m_subresourceRange.arrayLayersNum == rhi::constants::allRemainingArrayLayers
		? m_texture->GetDefinition().arrayLayers - m_subresourceRange.baseArrayLayer
		: m_subresourceRange.arrayLayersNum;
}

rhi::ETextureType RHITextureView::GetTextureType() const
{
	SPT_CHECK(m_texture);
	return m_texture->GetType();
}

void RHITextureView::SetName(const lib::HashedString& name)
{
	m_name.Set(name, reinterpret_cast<Uint64>(m_viewHandle), VK_OBJECT_TYPE_IMAGE_VIEW);
}

const lib::HashedString& RHITextureView::GetName() const
{
	return m_name.Get();
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// RHIMappedTexture =======================================================================

RHIMappedSurface::RHIMappedSurface(const RHITexture& texture, Byte* data, Uint32 bytesPerFragment, Uint32 mipIdx, const VkSubresourceLayout& layout)
	: m_texture(texture)
	, m_data(data)
	, m_bytesPerFragment(bytesPerFragment)
	, m_mipIdx(mipIdx)
	, m_layout(layout)
{
	SPT_CHECK(data != nullptr);
	SPT_CHECK(bytesPerFragment > 0);
}


//////////////////////////////////////////////////////////////////////////////////////////////////
// RHIMappedTexture =======================================================================

RHIMappedTexture::RHIMappedTexture(const RHITexture& texture)
	: m_texture(texture)
{
	SPT_CHECK(texture.GetDefinition().tiling == rhi::ETextureTiling::Linear);

	m_mappedPointer = m_texture.MapPtr();

	m_bytesPerFragment = static_cast<Uint32>(m_texture.GetFragmentSize());
	m_aspect           = RHIToVulkan::GetAspectFlags(rhi::GetFullAspectForFormat(texture.GetFormat()));
}

RHIMappedTexture::~RHIMappedTexture()
{
	if (m_mappedPointer)
	{
		m_texture.Unmap();
		m_mappedPointer = nullptr;
	}
}

RHIMappedSurface RHIMappedTexture::GetSurface(Uint32 mipLevel, Uint32 arrayLayer) const
{
	SPT_CHECK(mipLevel < m_texture.GetDefinition().mipLevels);
	SPT_CHECK(arrayLayer < m_texture.GetDefinition().arrayLayers);

	VkSubresourceLayout layout;

	VkImageSubresource subresource{};
	subresource.aspectMask = m_aspect;
    subresource.mipLevel   = mipLevel;
    subresource.arrayLayer = arrayLayer;

	vkGetImageSubresourceLayout(VulkanRHI::GetDeviceHandle(), m_texture.GetHandle(), OUT &subresource, OUT &layout);

	return RHIMappedSurface(m_texture, m_mappedPointer + layout.offset, m_bytesPerFragment, mipLevel, layout);
}

} // spt::vulkan