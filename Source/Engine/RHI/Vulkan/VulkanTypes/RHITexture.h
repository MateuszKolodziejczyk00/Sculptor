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

	Bool							HasUsage(rhi::ETextureUsage usage) const;

	const math::Vector3u&			GetResolution() const;
	math::Vector3u					GetMipResolution(Uint32 mipLevel) const;

	rhi::EFragmentFormat			GetFormat() const;

	rhi::ETextureType				GetType() const;
	
	const rhi::RHIAllocationInfo&	GetAllocationInfo() const;

	rhi::TextureFragmentInfo		GetFragmentInfo() const;
	Uint64							GetMipSize(Uint32 mipIdx) const;

	VkImage							GetHandle() const;

	Bool							IsGloballyReadable() const;

	// Currently not threadsafe
	Byte*							MapPtr() const;
	void							Unmap() const;

	void							SetName(const lib::HashedString& name);
	const lib::HashedString&		GetName() const;

	// Vulkan Helpers ======================================================================================

	static VkImageUsageFlags		GetVulkanTextureUsageFlags(rhi::ETextureUsage usageFlags);
	static VkFormat					GetVulkanFormat(rhi::EFragmentFormat format);

	static rhi::ETextureUsage		GetRHITextureUsageFlags(VkImageUsageFlags usageFlags);
	static rhi::EFragmentFormat		GetRHIFormat(VkFormat format);

private:

	Bool                             BindMemory(const rhi::RHIResourceAllocationDefinition& allocationDefinition);
	rhi::RHIResourceAllocationHandle ReleasePlacedAllocation();

	rhi::RHIResourceAllocationHandle DoPlacedAllocation(const rhi::RHIPlacedAllocationDefinition& placedAllocationDef);
	rhi::RHIResourceAllocationHandle DoCommittedAllocation(const rhi::RHICommittedAllocationDefinition& committedAllocation);

	rhi::TextureDefinition           m_definition;
	VkImage                          m_imageHandle;
	
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

	void								CopyUAVDescriptor(Byte* dst) const;
	void								CopySRVDescriptor(Byte* dst) const;

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


class RHI_API RHIMappedSurface
{
public:

	RHIMappedSurface(const RHITexture& texture, Byte* data, Uint32 bytesPerFragment, Uint32 mipIdx, const VkSubresourceLayout& layout);

	const RHITexture& GetTexture() const    { return m_texture; }
	Uint32            GetMipIdx()  const    { return m_mipIdx; }
	math::Vector3u    GetResolution() const { return GetTexture().GetMipResolution(GetMipIdx()); }

	template<typename TDataType>
	TDataType& At(const math::Vector3u& corrds) const
	{
		SPT_CHECK_MSG(sizeof(TDataType) == m_bytesPerFragment, "Invalid type size for texture {}: {} != {}", m_texture.GetName().ToString(), sizeof(TDataType), m_bytesPerFragment);

		SPT_CHECK_MSG(corrds.x() < m_texture.GetResolution().x() && corrds.y() < m_texture.GetResolution().y() && corrds.z() < m_texture.GetResolution().z(),
					  "Invalid coordinates for texture {}: ({}, {}, {})", m_texture.GetName().ToString(), corrds.x(), corrds.y(), corrds.z());

		return *reinterpret_cast<TDataType*>(m_data + corrds.x() * m_bytesPerFragment + corrds.y() * m_layout.rowPitch + corrds.z() * m_layout.depthPitch);
	}

	template<typename TDataType>
	TDataType& At(const math::Vector2u& coords) const
	{
		return At<TDataType>(math::Vector3u(coords.x(), coords.y(), 0u));
	}

	template<typename TDataType>
	TDataType& At(Uint32 coordsX) const
	{
		return At<TDataType>(math::Vector3u(coordsX, 0u, 0u));
	}

	lib::Span<Byte> GetMipData() const
	{
		return lib::Span<Byte>(m_data, m_layout.size);
	}

	lib::Span<Byte> GetRowData(Uint32 row, Uint32 depth) const
	{
		const Uint32 size   = m_texture.GetResolution().x() * m_bytesPerFragment;
		const Uint32 offset = static_cast<Uint32>(depth * m_layout.depthPitch + row * m_layout.rowPitch);

		return lib::Span<Byte>(m_data + offset, size);
	}

	Uint32 GetRowStride() const { return static_cast<Uint32>(m_layout.rowPitch); }

private:

	const RHITexture&   m_texture;
	Byte*               m_data = nullptr; // data after applying offset from layout
	Uint32              m_bytesPerFragment = 0u;

	Uint32              m_mipIdx = 0u;

	VkSubresourceLayout m_layout;
};


class RHI_API RHIMappedTexture
{
public:

	explicit RHIMappedTexture(const RHITexture& texture);
	~RHIMappedTexture();

	RHIMappedSurface GetSurface(Uint32 mipLevel, Uint32 arrayLayer) const;

private:

	const RHITexture&   m_texture;

	Byte*               m_mappedPointer = nullptr;

	Uint32              m_bytesPerFragment = 0u;

	VkImageAspectFlags  m_aspect = 0u;
};

} // spt::vulkan
