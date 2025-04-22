#pragma once

#include "RHIMacros.h"
#include "Vulkan/VulkanCore.h"
#include "SculptorCoreTypes.h"
#include "RHICore/RHITextureTypes.h"
#include "RHICore/RHIAllocationTypes.h"
#include "Vulkan/Debug/DebugUtils.h"


namespace spt::vulkan
{

struct RHI_API RHITextureReleaseTicket
{
	void ExecuteReleaseRHI();

	RHIResourceReleaseTicket<VkImage> handle;
	RHIResourceReleaseTicket<VmaAllocation> allocation;

#if SPT_RHI_DEBUG
	lib::HashedString name;
#endif // SPT_RHI_DEBUG
};


class RHI_API RHITexture
{
public:

	RHITexture();

	void							InitializeRHI(const rhi::TextureDefinition& definition, VkImage imageHandle, rhi::EMemoryUsage memoryUsage);
	void							InitializeRHI(const rhi::TextureDefinition& definition, const rhi::RHIResourceAllocationDefinition& allocationDef);
	void							ReleaseRHI();

	RHITextureReleaseTicket			DeferredReleaseRHI();

	Bool							IsValid() const;

	Bool							HasBoundMemory() const;

	Bool							IsPlacedAllocation() const;
	Bool							IsCommittedAllocation() const;

	rhi::RHIMemoryRequirements		GetMemoryRequirements() const;

	const rhi::TextureDefinition&	GetDefinition() const;

	const math::Vector3u&			GetResolution() const;
	math::Vector3u					GetMipResolution(Uint32 mipLevel) const;

	rhi::EFragmentFormat			GetFormat() const;

	rhi::ETextureType				GetType() const;
	
	const rhi::RHIAllocationInfo&	GetAllocationInfo() const;

	Uint64							GetFragmentSize() const;

	VkImage							GetHandle() const;

	void							SetName(const lib::HashedString& name);
	const lib::HashedString&		GetName() const;

	// Vulkan Helpers ======================================================================================

	static VkImageUsageFlags		GetVulkanTextureUsageFlags(rhi::ETextureUsage usageFlags);
	static VkFormat					GetVulkanFormat(rhi::EFragmentFormat format);

	static rhi::ETextureUsage		GetRHITextureUsageFlags(VkImageUsageFlags usageFlags);
	static rhi::EFragmentFormat		GetRHIFormat(VkFormat format);

private:

	void PostImageInitialized();
	void PreImageReleased();

	Bool                             BindMemory(const rhi::RHIResourceAllocationDefinition& allocationDefinition);
	rhi::RHIResourceAllocationHandle ReleasePlacedAllocation();

	rhi::RHIResourceAllocationHandle DoPlacedAllocation(const rhi::RHIPlacedAllocationDefinition& placedAllocationDef);
	rhi::RHIResourceAllocationHandle DoCommittedAllocation(const rhi::RHICommittedAllocationDefinition& committedAllocation);

	rhi::TextureDefinition           m_definition;
	VkImage                          m_imageHandle;
	
	rhi::ETextureType                m_type;
	
	rhi::RHIResourceAllocationHandle m_allocationHandle;
	rhi::RHIAllocationInfo           m_allocationInfo;

	DebugName m_name;

	friend class RHITextureMemoryOwner;
};


class RHI_API RHITextureMemoryOwner
{
protected:

	static Bool                             BindMemory(RHITexture& texture, const rhi::RHIResourceAllocationDefinition& allocationDefinition);
	static rhi::RHIResourceAllocationHandle ReleasePlacedAllocation(RHITexture& texture);
};


struct RHI_API RHITextureViewReleaseTicket
{
	void ExecuteReleaseRHI();

	RHIResourceReleaseTicket<VkImageView> handle;

#if SPT_RHI_DEBUG
	lib::HashedString name;
#endif // SPT_RHI_DEBUG
};


class RHI_API RHITextureView
{
public:

	RHITextureView();

	void								InitializeRHI(const RHITexture& texture, const rhi::TextureViewDefinition& viewDefinition);
	void								ReleaseRHI();

	RHITextureViewReleaseTicket			DeferredReleaseRHI();

	Bool								IsValid() const;
	
	VkImageView							GetHandle() const;

	const RHITexture*					GetTexture() const;

	math::Vector3u						GetResolution() const;
	math::Vector2u						GetResolution2D() const;

	rhi::EFragmentFormat				GetFormat() const;

	rhi::ETextureAspect					GetAspect() const;

	const rhi::TextureSubresourceRange&	GetSubresourceRange() const;

	Uint32								GetMipLevelsNum() const;
	Uint32								GetArrayLevelsNum() const;

	rhi::ETextureType					GetTextureType() const;

	void								SetName(const lib::HashedString& name);
	const lib::HashedString&			GetName() const;

private:

	VkImageView							m_viewHandle;

	rhi::TextureSubresourceRange		m_subresourceRange;

	const RHITexture*					m_texture;

	DebugName							m_name;
};

} // spt::vulkan