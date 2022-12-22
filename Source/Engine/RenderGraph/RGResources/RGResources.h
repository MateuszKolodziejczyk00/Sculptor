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


namespace spt::rg
{

enum class ERGTextureAccess
{
	Unknown,

	ColorRenderTarget,
	DepthRenderTarget,
	StencilRenderTarget,

	StorageWriteTexture,

	SampledTexture
};


enum class ERGBufferAccess
{
	Unknown,
	Read,
	Write,
	ReadWrite
};


enum class ERGResourceFlags : Flags32
{
	None = 0,
	External	= BIT(0),

	Default = None
};


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


struct RGTextureSubresourceAccessState
{
	RGTextureSubresourceAccessState()
		: lastAccessType(ERGTextureAccess::Unknown)
	{ }
	RGTextureSubresourceAccessState(ERGTextureAccess inLastAccessType, RGNodeHandle inLastAccessNode)
		: lastAccessType(inLastAccessType)
		, lastAccessNode(inLastAccessNode)
	{ }

	ERGTextureAccess		lastAccessType;
	RGNodeHandle	lastAccessNode;
};


struct RGTextureSubresource
{
	explicit RGTextureSubresource(Uint32 inArrayLayerIdx, Uint32 inMipMapIdx)
		: arrayLayerIdx(inArrayLayerIdx)
		, mipMapIdx(inMipMapIdx)
	{ }

	Uint32 arrayLayerIdx;
	Uint32 mipMapIdx;
};


class RGTextureAccessState
{
public:

	RGTextureAccessState(Uint32 textureMipsNum, Uint32 textureLayersNum)
		: m_textureMipsNum(textureMipsNum)
		, m_textureLayersNum(textureLayersNum)
	{
		m_subresourcesAccesses.resize(1);
		m_subresourcesAccesses[0] = RGTextureSubresourceAccessState(ERGTextureAccess::Unknown, nullptr);
	}

	Bool IsFullResource() const
	{
		return m_subresourcesAccesses.size() == 1;
	}

	RGTextureSubresourceAccessState& GetForFullResource()
	{
		SPT_CHECK(IsFullResource());
		return m_subresourcesAccesses[0];
	}

	RGTextureSubresourceAccessState& GetForSubresource(RGTextureSubresource subresource)
	{
		return GetForSubresource(subresource.arrayLayerIdx, subresource.mipMapIdx);
	}

	RGTextureSubresourceAccessState& GetForSubresource(Uint32 layerIdx, Uint32 mipMapIdx)
	{
		SPT_CHECK(mipMapIdx < m_textureMipsNum && layerIdx < m_textureLayersNum);

		if (IsFullResource())
		{
			return GetForFullResource();
		}

		return m_subresourcesAccesses[GetSubresourceIdx(layerIdx, mipMapIdx)];
	}

	template<typename TCallable>
	void ForEachSubresource(const rhi::TextureSubresourceRange& range, TCallable&& callable)
	{
		SPT_PROFILER_FUNCTION();

		const Uint32 lastLayerIdx = range.baseArrayLayer + range.arrayLayersNum;
		const Uint32 lastMipIdx = range.baseMipLevel + range.mipLevelsNum;

		for (Uint32 layerIdx = range.baseArrayLayer; layerIdx < lastLayerIdx; ++layerIdx)
		{
			for (Uint32 mipIdx = range.baseMipLevel; mipIdx < lastMipIdx; ++mipIdx)
			{
				callable(RGTextureSubresource(layerIdx, mipIdx));
			}
		}
	}

	template<typename TCallable>
	void ForEachSubresource(TCallable&& callable)
	{
		SPT_PROFILER_FUNCTION();

		for (Uint32 layerIdx = 0; layerIdx < m_textureLayersNum; ++layerIdx)
		{
			for (Uint32 mipIdx = 0; mipIdx < m_textureMipsNum; ++mipIdx)
			{
				callable(RGTextureSubresource(layerIdx, mipIdx));
			}
		}
	}

	void SetSubresourcesAccess(const RGTextureSubresourceAccessState& access, const rhi::TextureSubresourceRange& range)
	{
		SPT_PROFILER_FUNCTION();

		if (RangeContainsFullResource(range))
		{
			MergeTo(access);
			return;
		}

		if (IsFullResource())
		{
			BreakIntoSubresources();
		}

		ForEachSubresource(range,
						   [this, &access](RGTextureSubresource subresource)
						   {
							   GetForSubresource(subresource) = access;
						   });
	}

	Bool RangeContainsFullResource(const rhi::TextureSubresourceRange& range)
	{
		return range.baseMipLevel == 0
			&& range.baseArrayLayer == 0
			&& (range.mipLevelsNum == m_textureLayersNum || range.mipLevelsNum == rhi::constants::allRemainingMips)
			&& (range.arrayLayersNum == m_textureMipsNum || range.arrayLayersNum == rhi::constants::allRemainingArrayLayers);
	}

	void MergeTo(const RGTextureSubresourceAccessState& access)
	{
		m_subresourcesAccesses.resize(1);
		m_subresourcesAccesses[0] = access;
	}

	void BreakIntoSubresources()
	{
		if (IsFullResource())
		{
			const RGTextureSubresourceAccessState state = m_subresourcesAccesses[0];
			m_subresourcesAccesses.resize(m_textureLayersNum * m_textureMipsNum);
			std::fill(std::begin(m_subresourcesAccesses), std::end(m_subresourcesAccesses), state);
		}
	}

private:

	inline SizeType GetSubresourceIdx(Uint32 layerIdx, Uint32 mipMapIdx) const
	{
		return static_cast<SizeType>(layerIdx * m_textureMipsNum + mipMapIdx);
	}

	Uint32 m_textureMipsNum;
	Uint32 m_textureLayersNum;

	// For now we don't support different access masks
	lib::DynamicArray<RGTextureSubresourceAccessState> m_subresourcesAccesses;
};


class RGTexture : public RGResource
{
public:

	RGTexture(const RGResourceDef& resourceDefinition, const rhi::TextureDefinition& textureDefinition, const rhi::RHIAllocationInfo& allocationInfo)
		: RGResource(resourceDefinition)
		, m_textureDefinition(textureDefinition)
		, m_allocationInfo(allocationInfo)
		, m_accessState(textureDefinition.mipLevels, textureDefinition.arrayLayers)
		, m_releaseTransitionTarget(nullptr)
	{ }

	RGTexture(const RGResourceDef& resourceDefinition, lib::SharedPtr<rdr::Texture> texture)
		: RGResource(resourceDefinition)
		, m_textureDefinition(texture->GetRHI().GetDefinition())
		, m_allocationInfo(texture->GetRHI().GetAllocationInfo())
		, m_texture(texture)
		, m_accessState(texture->GetRHI().GetDefinition().mipLevels, texture->GetRHI().GetDefinition().arrayLayers)
		, m_releaseTransitionTarget(nullptr)
	{
		SPT_CHECK(IsExternal());
	}
	
	// Texture Definition ==================================================
	
	const rhi::TextureDefinition& GetTextureDefinition() const
	{
		return m_textureDefinition;
	}

	const rhi::RHIAllocationInfo& GetAllocationInfo() const
	{
		return m_allocationInfo;
	}

	// Texture Resource ====================================================

	Bool IsAcquired() const
	{
		return !!m_texture;
	}
	
	const lib::SharedPtr<rdr::Texture>& GetResource() const
	{
		SPT_CHECK(IsAcquired());
		return m_texture;
	}

	void AcquireResource(lib::SharedPtr<rdr::Texture> texture)
	{
		SPT_CHECK(!!m_texture);
		m_texture = std::move(texture);
	}

	lib::SharedPtr<rdr::Texture> ReleaseResource()
	{
		lib::SharedPtr<rdr::Texture> texture = std::move(m_texture);
		SPT_CHECK(!m_texture);
		return texture;
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

private:

	rhi::TextureDefinition m_textureDefinition;
	rhi::RHIAllocationInfo m_allocationInfo;

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

	RGTextureView(const RGResourceDef& resourceDefinition, RGTextureHandle texture, lib::SharedRef<rdr::TextureView> textureView)
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

	const rhi::TextureSubresourceRange& GetSubresourceRange() const
	{
		return IsExternal() ? m_textureView->GetRHI().GetSubresourceRange() : GetViewDefinition().subresourceRange;
	}

	const lib::SharedPtr<rdr::TextureView>& GetViewInstance() const
	{
		if (!!m_textureView)
		{
			m_textureView = CreateView();
		}

		SPT_CHECK(!!m_textureView);
		return m_textureView;
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
		, m_lastAccess(ERGBufferAccess::Unknown)
		, m_extractionDest(nullptr)
	{ }

	RGBuffer(const RGResourceDef& resourceDefinition, lib::SharedPtr<rdr::Buffer> bufferInstance)
		: RGResource(resourceDefinition)
		, m_bufferInstance(std::move(bufferInstance))
		, m_lastAccess(ERGBufferAccess::Unknown)
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

	// Resource ============================================================

	Bool IsAcquired() const
	{
		return !!m_bufferInstance;
	}
	
	void AcquireResource(lib::SharedPtr<rdr::Buffer> buffer)
	{
		SPT_CHECK(!m_bufferInstance);
	}

	lib::SharedPtr<rdr::Buffer> ReleaseResource()
	{
		lib::SharedPtr<rdr::Buffer> resource = std::move(m_bufferInstance);
		SPT_CHECK(m_bufferInstance);
		return resource;
	}

	const lib::SharedPtr<rdr::Buffer>& GetResource() const
	{
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

	ERGBufferAccess GetLastAccessType() const
	{
		return m_lastAccess;
	}

	void SetLastAccessType(ERGBufferAccess access)
	{
		m_lastAccess = access;
	}

private:

	rhi::BufferDefinition	m_bufferDef;
	rhi::RHIAllocationInfo	m_allocationInfo;

	lib::SharedPtr<rdr::Buffer> m_bufferInstance;

	RGNodeHandle	m_lastAccessNode;
	ERGBufferAccess	m_lastAccess;

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
	{
		SPT_CHECK(m_buffer.IsValid());
	}

	RGBufferView(const RGResourceDef& resourceDefinition, RGBufferHandle buffer, lib::SharedPtr<rdr::BufferView> bufferView)
		: RGResource(resourceDefinition)
		, m_buffer(buffer)
		, m_offset(bufferView->GetOffset())
		, m_size(bufferView->GetSize())
		, m_bufferViewInstance(std::move(bufferView))
	{
		SPT_CHECK(m_buffer.IsValid());
		SPT_CHECK(IsExternal());
		SPT_CHECK(m_offset + m_size <= m_bufferViewInstance->GetBuffer()->GetRHI().GetSize());
	}

	Uint64 GetOffset() const
	{
		return m_offset;
	}

	Uint64 GetSize() const
	{
		return m_size;
	}

	RGBufferHandle GetBuffer() const
	{
		return m_buffer;
	}

	const lib::SharedPtr<rdr::BufferView>& GetBufferViewInstance() const
	{
		if (!m_bufferViewInstance)
		{
			const lib::SharedPtr<rdr::Buffer> owningBufferInstance = m_buffer->GetResource();
			SPT_CHECK(!!owningBufferInstance);

			m_bufferViewInstance = owningBufferInstance->CreateView(m_offset, m_size);
		}

		SPT_CHECK(!!m_bufferViewInstance);
		return m_bufferViewInstance;
	}

private:

	RGBufferHandle m_buffer;

	Uint64 m_offset;
	Uint64 m_size;

	mutable lib::SharedPtr<rdr::BufferView> m_bufferViewInstance;
};

} // spt::rg


namespace std
{

template<>
struct hash<spt::rg::RGTextureAccessState>
{
	size_t operator()(const spt::rg::RGTextureSubresourceAccessState& accessState) const
	{
		return spt::lib::HashCombine(accessState.lastAccessType, accessState.lastAccessNode);
	}
};

} // std