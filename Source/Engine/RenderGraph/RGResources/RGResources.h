#pragma once

#include "SculptorCoreTypes.h"
#include "RenderGraphTypes.h"
#include "RGResources/RGTrackedObject.h"
#include "RGResources/RGResourceHandles.h"
#include "Types/Texture.h"
#include "Types/Buffer.h"
#include "RHICore/RHITextureTypes.h"
#include "RHICore/RHIAllocationTypes.h"
#include "RHICore/RHIBufferTypes.h"
#include "RHICore/RHISynchronizationTypes.h"
#include "RHICore/RHIShaderTypes.h"
#include "Utility/Templates/TypeStorage.h"
#include "RGResourceTypes.h"


namespace spt::rg
{


struct RGResourceDef
{
	RGResourceDef()
		: flags(ERGResourceFlags::None)
	{ }

	RenderGraphDebugName	name;
	ERGResourceFlags		flags;
};


class RGResource abstract : public RGTrackedObject
{
public:

	explicit RGResource(const RGResourceDef& definition)
		: m_name(definition.name)
		, m_flags(definition.flags)
	{ }

	const lib::HashedString& GetName() const
	{
		return m_name.Get();
	}

	ERGResourceFlags GetFlags() const
	{
		return m_flags;
	}

	Bool HasAcquireNode() const
	{
		return m_acquireNode.IsValid();
	}

	void SetAcquireNode(RGNodeHandle node)
	{
		m_acquireNode = node;
	}

	RGNodeHandle GetAcquireNode() const
	{
		return m_acquireNode;
	}

	Bool HasReleaseNode() const
	{
		return m_releaseNode.IsValid();
	}

	void SetReleaseNode(RGNodeHandle node)
	{
		m_releaseNode = node;
	}

	RGNodeHandle GetReleaseNode() const
	{
		return m_releaseNode;
	}

	Bool IsExternal() const
	{
		return lib::HasAnyFlag(GetFlags(), ERGResourceFlags::External);
	}

private:

	RenderGraphDebugName	m_name;
	ERGResourceFlags		m_flags;

	// Node that initializes this resource
	RGNodeHandle	m_acquireNode;
	// Node that releases this resource
	RGNodeHandle	m_releaseNode;
};


struct TextureDef
{
	TextureDef()
	{ }
	
	TextureDef(const rhi::RHIResolution& inResolution, rhi::EFragmentFormat inFormat)
		: resolution(inResolution)
		, format(inFormat)
	{ }

	rhi::TextureDefinition ToRHI() const
	{
		rhi::TextureDefinition rhiDefinition;
		rhiDefinition.resolution	= resolution;
		rhiDefinition.format		= format;
		rhiDefinition.samples		= samples;
		rhiDefinition.mipLevels		= mipLevels;
		rhiDefinition.arrayLayers	= arrayLayers;
		rhiDefinition.type			= type;

		return rhiDefinition;
	}

	rhi::RHIResolution		resolution;
	rhi::EFragmentFormat	format = rhi::EFragmentFormat::None;
	Uint8					samples = 1u;
	Uint32					mipLevels = 1u;
	Uint32					arrayLayers = 1u;
	rhi::ETextureType		type = rhi::ETextureType::Auto;
};


class RGTexture : public RGResource
{
public:

	RGTexture(const RGResourceDef& resourceDefinition, const TextureDef& textureDefinition, const std::optional<rhi::RHIAllocationInfo>& allocationInfo)
		: RGResource(resourceDefinition)
		, m_textureDefinition(textureDefinition.ToRHI())
		, m_allocationInfo(allocationInfo)
		, m_isAcquired(false)
		, m_accessState(textureDefinition.mipLevels, textureDefinition.arrayLayers)
		, m_extractionDest(nullptr)
		, m_releaseTransitionTarget(nullptr)
	{ }

	RGTexture(const RGResourceDef& resourceDefinition, lib::SharedPtr<rdr::Texture> texture)
		: RGResource(resourceDefinition)
		, m_textureDefinition(texture->GetRHI().GetDefinition())
		, m_allocationInfo(texture->GetRHI().GetAllocationInfo())
		, m_isAcquired(true)
		, m_texture(texture)
		, m_accessState(texture->GetRHI().GetDefinition().mipLevels, texture->GetRHI().GetDefinition().arrayLayers)
		, m_extractionDest(nullptr)
		, m_releaseTransitionTarget(nullptr)
	{
		SPT_CHECK(IsExternal());
	}
	
	// Texture Definition ==================================================
	
	rg::TextureDef GetTextureDefinition() const
	{
		rg::TextureDef def;
		def.resolution	= m_textureDefinition.resolution;
		def.format		= m_textureDefinition.format;
		def.samples		= m_textureDefinition.samples;
		def.mipLevels	= m_textureDefinition.mipLevels;
		def.arrayLayers	= m_textureDefinition.arrayLayers;
		def.type		= m_textureDefinition.type;

		return def;
	}

	const rhi::TextureDefinition& GetTextureRHIDefinition() const
	{
		return m_textureDefinition;
	}

	const std::optional<rhi::RHIAllocationInfo>& GetOptionalAllocationInfo() const
	{
		return m_allocationInfo;
	}

	const rhi::RHIAllocationInfo& GetAllocationInfo() const
	{
		return *m_allocationInfo;
	}

	rhi::ETextureUsage GetUsage() const
	{
		return GetTextureRHIDefinition().usage;
	}

	const math::Vector3u& GetResolution() const
	{
		return GetTextureRHIDefinition().resolution.AsVector();
	}

	math::Vector3u GetMipResolution(Uint32 mipLevel) const
	{
		return math::Utils::ComputeMipResolution(GetResolution(), mipLevel);
	}

	Uint64 GetMipSize(Uint32 mipLevel) const
	{
		const math::Vector3u mipResolution = GetMipResolution(mipLevel);
		return mipResolution.x() * mipResolution.y() * mipResolution.z() * rhi::GetFragmentSize(GetFormat());
	}

	math::Vector2u GetResolution2D() const
	{
		return GetResolution().head<2>();
	}

	Uint32 GetMipLevelsNum() const
	{
		return GetTextureDefinition().mipLevels;
	}

	rhi::EFragmentFormat GetFormat() const
	{
		return GetTextureDefinition().format;
	}

	Bool TryAppendUsage(rhi::ETextureUsage usage)
	{
		if (!IsExternal())
		{
			lib::AddFlag(m_textureDefinition.usage, usage);
		}

		return lib::HasAllFlags(GetUsage(), usage);
	}

	// Texture Resource ====================================================

	Bool IsAcquired() const
	{
		return m_isAcquired;
	}
	
	const lib::SharedPtr<rdr::Texture>& GetResource(Bool onlyIfAcquired = true) const
	{
		SPT_CHECK(!onlyIfAcquired || IsAcquired());
		return m_texture;
	}

	const rhi::RHITexture& GetRHI() const
	{
		SPT_CHECK(IsAcquired());
		return m_texture->GetRHI();
	}

	void AcquireResource(lib::SharedPtr<rdr::Texture> texture)
	{
		SPT_CHECK(!IsAcquired());
		m_texture = std::move(texture);
		m_isAcquired = true;
	}

	lib::SharedPtr<rdr::Texture> ReleaseResource()
	{
		m_isAcquired = false;
		return m_texture;
	}

	// Extraction ==========================================================

	void SetExtractionDestination(lib::SharedPtr<rdr::Texture>& destination)
	{
		SPT_CHECK_MSG(!IsExtracted(), "Texture cannot be extracted twice");
		m_extractionDest = &destination;
	}

	Bool IsExtracted() const
	{
		return !!m_extractionDest;
	}

	lib::SharedPtr<rdr::Texture>& GetExtractionDestChecked() const
	{
		SPT_CHECK(IsExtracted());
		return *m_extractionDest;
	}

	// Access State ========================================================

	RGTextureAccessState& GetAccessState()
	{
		return m_accessState;
	}

	// Relase ==============================================================

	void SetReleaseTransitionTarget(const rhi::BarrierTextureTransitionDefinition* transitionTarget)
	{
		SPT_CHECK_MSG(!m_releaseTransitionTarget, "Release transition already defined");
		m_releaseTransitionTarget = transitionTarget;
	}

	const rhi::BarrierTextureTransitionDefinition* GetReleaseTransitionTarget() const
	{
		return m_releaseTransitionTarget;
	}

	// Properties ==========================================================

	void AddUsageForAccess(ERGTextureAccess access)
	{
		Bool success = true;
		switch (access)
		{
		case ERGTextureAccess::ColorRenderTarget:
			success |= TryAppendUsage(rhi::ETextureUsage::ColorRT);
			break;
		case ERGTextureAccess::DepthRenderTarget:
			success |= TryAppendUsage(rhi::ETextureUsage::DepthSetncilRT);
			break;
		case ERGTextureAccess::StencilRenderTarget:
			success |= TryAppendUsage(rhi::ETextureUsage::DepthSetncilRT);
			break;
		case ERGTextureAccess::StorageWriteTexture:
			success |= TryAppendUsage(rhi::ETextureUsage::StorageTexture);
			break;
		case ERGTextureAccess::SampledTexture:
			success |= TryAppendUsage(rhi::ETextureUsage::SampledTexture);
			break;
		case ERGTextureAccess::TransferSource:
			success |= TryAppendUsage(rhi::ETextureUsage::TransferSource);
			break;
		case ERGTextureAccess::TransferDest:
			success |= TryAppendUsage(rhi::ETextureUsage::TransferDest);

			break;
		}

		SPT_CHECK(success);
	}

	void SelectAllocationStrategy()
	{
		if (!m_allocationInfo.has_value())
		{
			m_allocationInfo = rhi::EMemoryUsage::GPUOnly;
		}
	}

private:

	rhi::TextureDefinition					m_textureDefinition;
	std::optional<rhi::RHIAllocationInfo>	m_allocationInfo;

	Bool m_isAcquired;

	lib::SharedPtr<rdr::Texture> m_texture;

	RGTextureAccessState m_accessState;

	lib::SharedPtr<rdr::Texture>*					m_extractionDest;
	const rhi::BarrierTextureTransitionDefinition*	m_releaseTransitionTarget;
};


class RGTextureView : public RGResource
{
public:

	RGTextureView(const RGResourceDef& resourceDefinition, RGTextureHandle texture, const rhi::TextureViewDefinition& viewDefinition)
		: RGResource(resourceDefinition)
		, m_texture(texture)
		, m_viewDef(viewDefinition)
	{ }

	RGTextureView(const RGResourceDef& resourceDefinition, RGTextureHandle texture, const lib::SharedRef<rdr::TextureView>& textureView)
		: RGResource(resourceDefinition)
		, m_texture(texture)
		, m_textureView(textureView)
	{
		SPT_CHECK(IsExternal());
	}

	RGTextureHandle GetTexture() const
	{
		return m_texture;
	}

	rg::TextureDef GetTextureDefinition() const
	{
		SPT_CHECK(m_texture.IsValid());
		return m_texture->GetTextureDefinition();
	}

	const rhi::TextureDefinition& GetTextureRHIDefinition() const
	{
		SPT_CHECK(m_texture.IsValid());
		return m_texture->GetTextureRHIDefinition();
	}

	const rhi::TextureSubresourceRange& GetSubresourceRange() const
	{
		return IsExternal() ? m_textureView->GetRHI().GetSubresourceRange() : GetViewDefinition().subresourceRange;
	}

	math::Vector3u GetResolution() const
	{
		return m_texture->GetMipResolution(GetSubresourceRange().baseMipLevel);
	}

	math::Vector2u GetResolution2D() const
	{
		return GetResolution().head<2>();
	}

	Uint64 GetMipSize() const
	{
		return m_texture->GetMipSize(GetSubresourceRange().baseMipLevel);
	}


	Uint32 GetMipLevelsNum() const
	{
		const rhi::TextureSubresourceRange& range = GetSubresourceRange();
		return range.mipLevelsNum == rhi::constants::allRemainingMips
			? m_texture->GetMipLevelsNum() - range.baseMipLevel
			: range.mipLevelsNum;
	}

	Uint32 GetArrayLevelsNum() const
	{
		const rhi::TextureSubresourceRange& range = GetSubresourceRange();
		return range.arrayLayersNum == rhi::constants::allRemainingArrayLayers
			? m_texture->GetTextureDefinition().arrayLayers - range.baseArrayLayer
			: range.arrayLayersNum;
	}

	rhi::EFragmentFormat GetFormat() const
	{
		return GetTexture()->GetFormat();
	}

	const rhi::RHIAllocationInfo& GetAllocationInfo() const
	{
		return GetTexture()->GetAllocationInfo();
	}

	const lib::SharedPtr<rdr::TextureView>& GetViewInstance() const
	{
		if (!m_textureView)
		{
			m_textureView = CreateView();
		}

		SPT_CHECK(!!m_textureView);
		return m_textureView;
	}

	// Texture Resource ====================================================

	Bool IsAcquired() const
	{
		return m_textureView && m_texture->IsAcquired();
	}
	
	const lib::SharedPtr<rdr::TextureView>& GetResource(Bool onlyIfAcquired = true) const
	{
		SPT_CHECK(!onlyIfAcquired || IsAcquired());
		return m_textureView;
	}

	void AcquireResource()
	{
		// texture views are not reused so they don't need to be released after acquire
		SPT_CHECK(!IsAcquired());
		m_textureView = CreateView();
	}

	const rhi::RHITextureView& GetRHI() const
	{
		SPT_CHECK(IsAcquired());
		return m_textureView->GetRHI();
	}

private:

	lib::SharedPtr<rdr::TextureView> CreateView() const
	{
		SPT_CHECK(m_texture.IsValid());
		SPT_CHECK(m_texture->IsAcquired());

		const lib::SharedPtr<rdr::Texture>& textureInstance = m_texture->GetResource();
		SPT_CHECK(!!textureInstance);

		return textureInstance->CreateView(RENDERER_RESOURCE_NAME(GetName()), GetViewDefinition());
	}

	const rhi::TextureViewDefinition& GetViewDefinition() const
	{
		SPT_CHECK_MSG(!IsExternal(), "View Definition is not valid for external texture views");
		return m_viewDef;
	}

	RGTextureHandle				m_texture;
	rhi::TextureViewDefinition	m_viewDef;

	mutable lib::SharedPtr<rdr::TextureView> m_textureView;
};


class RGBuffer : public RGResource
{
public:

	RGBuffer(const RGResourceDef& resourceDefinition, const rhi::BufferDefinition& definition, const rhi::RHIAllocationInfo& allocationInfo)
		: RGResource(resourceDefinition)
		, m_bufferDef(definition)
		, m_allocationInfo(allocationInfo)
		, m_isAcquired(false)
		, m_extractionDest(nullptr)
	{ }

	RGBuffer(const RGResourceDef& resourceDefinition, lib::SharedPtr<rdr::Buffer> bufferInstance)
		: RGResource(resourceDefinition)
		, m_isAcquired(true)
		, m_bufferInstance(std::move(bufferInstance))
		, m_extractionDest(nullptr)
	{
		SPT_CHECK(lib::HasAnyFlag(GetFlags(), ERGResourceFlags::External));
		SPT_CHECK(IsAcquired());
	}

	const rhi::BufferDefinition& GetBufferDefinition() const
	{
		SPT_CHECK_MSG(!IsExternal(), "Definition is invalid for external buffers");
		return m_bufferDef;
	}

	const rhi::RHIAllocationInfo& GetAllocationInfo() const
	{
		SPT_CHECK_MSG(!IsExternal(), "Allocation Info is invalid for external buffers");
		return m_allocationInfo;
	}

	rhi::EBufferUsage GetUsageFlags() const
	{
		return IsExternal() ? m_bufferInstance->GetRHI().GetUsage() : GetBufferDefinition().usage;
	}

	Bool AllowsHostWrites() const
	{
		if (IsExternal())
		{
			return m_bufferInstance->GetRHI().CanMapMemory();
		}
		else
		{
			return m_allocationInfo.memoryUsage != rhi::EMemoryUsage::GPUOnly;
		}
	}

	// Resource ============================================================

	Bool IsAcquired() const
	{
		return !!m_bufferInstance;
	}
	
	void AcquireResource(lib::SharedPtr<rdr::Buffer> buffer)
	{
		SPT_CHECK(!m_bufferInstance);
		m_bufferInstance = std::move(buffer);
		m_isAcquired = true;
	}

	lib::SharedPtr<rdr::Buffer> ReleaseResource()
	{
		m_isAcquired = false;
		return m_bufferInstance;
	}

	const lib::SharedPtr<rdr::Buffer>& GetResource(Bool onlyIfAcquired = true) const
	{
		SPT_CHECK(!onlyIfAcquired || IsAcquired());
		return m_bufferInstance;
	}

	// Extraction ==========================================================

	Bool IsExtracted() const
	{
		return !!m_extractionDest;
	}

	lib::SharedPtr<rdr::Buffer>* GetExtractionDest() const
	{
		return m_extractionDest;
	}

	void SetExtractionDest(lib::SharedPtr<rdr::Buffer>* dest)
	{
		SPT_CHECK_MSG(!IsExtracted(), "This resource was already extracted");
		m_extractionDest = dest;
	}

	// Access Synchronization ==============================================

	RGNodeHandle GetLastAccessNode() const
	{
		return m_lastAccessNode;
	}

	void SetLastAccessNode(RGNodeHandle node)
	{
		m_lastAccessNode = node;
	}

private:

	rhi::BufferDefinition	m_bufferDef;
	rhi::RHIAllocationInfo	m_allocationInfo;

	Bool m_isAcquired;

	RGNodeHandle			m_lastAccessNode;

	lib::SharedPtr<rdr::Buffer> m_bufferInstance;

	lib::SharedPtr<rdr::Buffer>* m_extractionDest;
};


class RGBufferView : public RGResource
{
public:

	RGBufferView(const RGResourceDef& resourceDefinition, RGBufferHandle buffer, Uint64 offset, Uint64 size)
		: RGResource(resourceDefinition)
		, m_buffer(buffer)
		, m_offset(offset)
		, m_size(size)
		, m_lastAccess(ERGBufferAccess::Unknown)
		, m_lastAccessPipelineStages(rhi::EPipelineStage::None)
		, m_hasValidInstance(false)
	{
		SPT_CHECK(m_buffer.IsValid());
	}

	RGBufferView(const RGResourceDef& resourceDefinition, RGBufferHandle buffer, const rdr::BufferView& bufferView)
		: RGResource(resourceDefinition)
		, m_buffer(buffer)
		, m_offset(bufferView.GetOffset())
		, m_size(bufferView.GetSize())
		, m_lastAccess(ERGBufferAccess::Unknown)
		, m_lastAccessPipelineStages(rhi::EPipelineStage::None)
		, m_bufferViewInstance(bufferView)
		, m_hasValidInstance(true)
	{
		SPT_CHECK(m_buffer.IsValid());
		SPT_CHECK(IsExternal());
	}

	~RGBufferView()
	{
		if (m_hasValidInstance)
		{
			m_bufferViewInstance.Destroy();
		}
	}

	Uint64 GetOffset() const
	{
		return m_offset;
	}

	Uint64 GetSize() const
	{
		return m_size;
	}

	rhi::EBufferUsage GetUsageFlags() const
	{
		return m_buffer->GetUsageFlags();
	}

	RGBufferHandle GetBuffer() const
	{
		return m_buffer;
	}

	Bool AllowsHostWrites() const
	{
		return m_buffer->AllowsHostWrites();
	}

	const rhi::BufferDefinition& GetBufferDefinition() const
	{
		return m_buffer->GetBufferDefinition();
	}

	const rdr::BufferView& GetResource(Bool onlyIfAcquired = true) const
	{
		SPT_CHECK(!onlyIfAcquired || m_buffer->IsAcquired());
		if (!m_hasValidInstance)
		{
			const lib::SharedPtr<rdr::Buffer> owningBufferInstance = m_buffer->GetResource(onlyIfAcquired);
			SPT_CHECK(!!owningBufferInstance);

			m_bufferViewInstance.Construct(lib::Ref(owningBufferInstance), m_offset, m_size);
			m_hasValidInstance = true;
		}

		return m_bufferViewInstance.Get();
	}

	// Access Synchronization ==============================================

	RGNodeHandle GetLastAccessNode() const
	{
		return m_lastAccessNode;
	}

	void SetLastAccessNode(RGNodeHandle node)
	{
		m_lastAccessNode = node;
	}

	ERGBufferAccess GetLastAccessType() const
	{
		return m_lastAccess;
	}

	void SetLastAccessType(ERGBufferAccess access)
	{
		m_lastAccess = access;
	}

	rhi::EPipelineStage GetLastAccessPipelineStages() const
	{
		return m_lastAccessPipelineStages;
	}

	void SetLastAccessPipelineStages(rhi::EPipelineStage stages)
	{
		m_lastAccessPipelineStages = stages;
	}

private:

	RGBufferHandle m_buffer;

	Uint64 m_offset;
	Uint64 m_size;

	RGNodeHandle			m_lastAccessNode;
	ERGBufferAccess			m_lastAccess;
	rhi::EPipelineStage		m_lastAccessPipelineStages;

	mutable lib::TypeStorage<rdr::BufferView>	m_bufferViewInstance;
	mutable Bool								m_hasValidInstance;
};

} // spt::rg


namespace spt::lib
{

template<>
struct Hasher<rg::RGTextureAccessState>
{
	size_t operator()(const rg::RGTextureSubresourceAccessState& accessState) const
	{
		return HashCombine(accessState.lastAccessType, accessState.lastAccessNode);
	}
};

} // spt::lib