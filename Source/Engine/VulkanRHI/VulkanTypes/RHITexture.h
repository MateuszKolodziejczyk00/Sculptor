#pragma once

#include "VulkanRHIMacros.h"
#include "Vulkan.h"
#include "SculptorCoreTypes.h"
#include "RHITextureTypes.h"
#include "Debug/DebugUtils.h"


namespace spt::rhi
{
struct RHIAllocationInfo;
}


namespace spt::vulkan
{

class VULKAN_RHI_API RHITexture
{
public:

	RHITexture();

	void							InitializeRHI(const rhi::TextureDefinition& definition, VkImage imageHandle);
	void							InitializeRHI(const rhi::TextureDefinition& definition, const rhi::RHIAllocationInfo& allocation);
	void							ReleaseRHI();

	Bool							IsValid() const;

	const rhi::TextureDefinition&	GetDefinition() const;

	VkImage							GetHandle() const;

	void							SetName(const lib::HashedString& name);
	const lib::HashedString&		GetName() const;

	// Vulkan Helpers ======================================================================================

	static VkImageUsageFlags		GetVulkanTextureUsageFlags(Flags32 usageFlags);
	static VkFormat					GetVulkanFormat(rhi::EFragmentFormat format);

	static Flags32					GetRHITextureUsageFlags(VkImageUsageFlags usageFlags);
	static rhi::EFragmentFormat		GetRHIFormat(VkFormat format);

private:

	void							PostImageInitialized();
	void							PreImageReleased();

	rhi::TextureDefinition			m_definition;

	VkImage							m_imageHandle;

	VmaAllocation					m_allocation;

	DebugName						m_name;
};


class VULKAN_RHI_API RHITextureView
{
public:

	RHITextureView();

	void							InitializeRHI(const RHITexture& texture, const rhi::TextureViewDefinition& viewDefinition);
	void							ReleaseRHI();

	Bool							IsValid() const;
	
	VkImageView						GetHandle() const;

	const RHITexture*				GetTexture() const;

	void							SetName(const lib::HashedString& name);
	const lib::HashedString&		GetName() const;

private:

	VkImageView						m_viewHandle;

	const RHITexture*				m_texture;

	DebugName						m_name;
};

}