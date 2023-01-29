#pragma once

#include "RHIMacros.h"
#include "Vulkan/VulkanCore.h"
#include "SculptorCoreTypes.h"
#include "RHICore/RHITextureTypes.h"
#include "RHICore/RHIAllocationTypes.h"
#include "Vulkan/Debug/DebugUtils.h"


namespace spt::vulkan
{

class RHI_API RHITexture
{
public:

	RHITexture();

	void							InitializeRHI(const rhi::TextureDefinition& definition, VkImage imageHandle, rhi::EMemoryUsage memoryUsage);
	void							InitializeRHI(const rhi::TextureDefinition& definition, const rhi::RHIAllocationInfo& allocationDef);
	void							ReleaseRHI();

	Bool							IsValid() const;

	const rhi::TextureDefinition&	GetDefinition() const;

	const math::Vector3u&			GetResolution() const;
	math::Vector3u					GetMipResolution(Uint32 mipLevel) const;
	
	const rhi::RHIAllocationInfo&	GetAllocationInfo() const;

	VkImage							GetHandle() const;

	void							SetName(const lib::HashedString& name);
	const lib::HashedString&		GetName() const;

	// Vulkan Helpers ======================================================================================

	static VkImageUsageFlags		GetVulkanTextureUsageFlags(rhi::ETextureUsage usageFlags);
	static VkFormat					GetVulkanFormat(rhi::EFragmentFormat format);

	static rhi::ETextureUsage		GetRHITextureUsageFlags(VkImageUsageFlags usageFlags);
	static rhi::EFragmentFormat		GetRHIFormat(VkFormat format);

private:

	void							PostImageInitialized();
	void							PreImageReleased();

	rhi::TextureDefinition			m_definition;
	rhi::RHIAllocationInfo			m_allocationInfo;

	VkImage							m_imageHandle;

	VmaAllocation					m_allocation;

	DebugName						m_name;
};


class RHI_API RHITextureView
{
public:

	RHITextureView();

	void								InitializeRHI(const RHITexture& texture, const rhi::TextureViewDefinition& viewDefinition);
	void								ReleaseRHI();

	Bool								IsValid() const;
	
	VkImageView							GetHandle() const;

	const RHITexture*					GetTexture() const;

	const rhi::TextureSubresourceRange&	GetSubresourceRange() const;

	void								SetName(const lib::HashedString& name);
	const lib::HashedString&			GetName() const;

private:

	VkImageView							m_viewHandle;

	rhi::TextureSubresourceRange		m_subresourceRange;

	const RHITexture*					m_texture;

	DebugName							m_name;
};

}