#pragma once

#include "SculptorCoreTypes.h"
#include "RenderGraphTypes.h"
#include "RGResources/RGTrackedResource.h"
#include "RHICore/RHITextureTypes.h"
#include "RHICore/RHIAllocationTypes.h"
#include "RHICore/RHIBufferTypes.h"
#include "Types/Texture.h"
#include "RGResources/RGNode.h"


namespace spt::rg
{

enum class ERGAccess
{
	Unknown
};


enum class ERGResourceType
{
	TextureView,
	Buffer
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


class RGResource abstract : public RGTrackedResource
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

private:

	RenderGraphDebugName	m_name;
	ERGResourceFlags		m_flags;
};


struct RGTextureSubresourcesRange
{
	explicit RGTextureSubresourcesRange(Uint32 inFirstMipMapIdx = 0, Uint32 mipMapsNum = 1, Uint32 inFirstLayerIdx = 0, Uint32 inLayersNum = 1)
		: firstMipMapIdx(inFirstMipMapIdx)
		, mipMapsNum(mipMapsNum)
		, firstLayerIdx(inFirstLayerIdx)
		, layersNum(inLayersNum)
	{ }

	Uint32 firstMipMapIdx;
	Uint32 mipMapsNum;
	Uint32 firstLayerIdx;
	Uint32 layersNum;
};


struct RGTextureSubresourceAccessState
{
	RGTextureSubresourceAccessState()
		: lastAccess(ERGAccess::Unknown)
	{ }

	ERGAccess		lastAccess;
	RGNodeHandle	lastProducerNode;
};


class RGTextureAccessState
{
public:

	RGTextureAccessState(Uint32 inTextureMipsNum, Uint32 inTextureLayersNum)
		: textureMipsNum(inTextureLayersNum)
		, textureLayersNum(inTextureLayersNum)
	{ }

	Bool IsFullResource() const
	{
		return m_subresourcesAccesses.size() == 1;
	}

	RGTextureSubresourceAccessState& GetForSubresource(Uint32 mipMapIdx, Uint32 layerIdx)
	{
		SPT_CHECK(mipMapIdx < textureMipsNum && layerIdx < textureLayersNum);

		if (IsFullResource())
		{
			return m_subresourcesAccesses[0];
		}

		return m_subresourcesAccesses[GetSubresourceIdx(mipMapIdx, layerIdx)];
	}

	void SetSubresourcesAccess(const RGTextureSubresourceAccessState& access, const RGTextureSubresourcesRange& range)
	{
		if (IsFullResource())
		{
			if (IsRangeForFullResource(range))
			{
				m_subresourcesAccesses[0] = access;
				return;
			}
			else
			{
				m_subresourcesAccesses.resize(static_cast<SizeType>(textureLayersNum * textureMipsNum));
			}
		}

		const Uint32 lastLayerIdx = range.firstLayerIdx + range.layersNum;
		const Uint32 lastMipIdx = range.firstMipMapIdx + range.mipMapsNum;

		for (Uint32 layerIdx = range.firstLayerIdx; layerIdx < lastLayerIdx; ++layerIdx)
		{
			for (Uint32 mipIdx = range.firstMipMapIdx; mipIdx < lastMipIdx; ++mipIdx)
			{
				m_subresourcesAccesses[GetSubresourceIdx(mipIdx, layerIdx)] = access;
			}
		}
	}

	void MergeTo(const RGTextureSubresourceAccessState& access)
	{
		m_subresourcesAccesses.resize(1);
		m_subresourcesAccesses[0] = access;
	}

private:

	inline SizeType GetSubresourceIdx(Uint32 mipMapIdx, Uint32 layerIdx) const
	{
		return static_cast<SizeType>(layerIdx * textureMipsNum + mipMapIdx);
	}

	Bool IsRangeForFullResource(const RGTextureSubresourcesRange& range) const
	{
		return range.firstLayerIdx == 0
			&& range.firstMipMapIdx == 0
			&& range.layersNum == textureLayersNum
			&& range.mipMapsNum == textureMipsNum;
	}

	Uint32 textureMipsNum;
	Uint32 textureLayersNum;
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
		, m_extractionDest(nullptr)
	{ }

	RGTexture(const RGResourceDef& resourceDefinition, lib::SharedPtr<rdr::Texture> texture)
		: RGResource(resourceDefinition)
		, m_textureDefinition(texture->GetRHI().GetDefinition())
		, m_allocationInfo(texture->GetRHI().GetAllocationInfo())
		, m_texture(texture)
		, m_accessState(texture->GetRHI().GetDefinition().mipLevels, texture->GetRHI().GetDefinition().arrayLayers)
		, m_extractionDest(nullptr)
	{
		SPT_CHECK(lib::HasAnyFlag(GetFlags(), ERGResourceFlags::External));
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

private:

	rhi::TextureDefinition m_textureDefinition;
	rhi::RHIAllocationInfo m_allocationInfo;

	lib::SharedPtr<rdr::Texture> m_texture;

	RGTextureAccessState m_accessState;

	lib::SharedPtr<rdr::Texture>* m_extractionDest;
};

using RGTextureHandle = RGResourceHandle<RGTexture>;


class RGTextureView
{
public:

	RGTextureView(RGTextureHandle texture, const rhi::TextureViewDefinition& viewDefinition)
		: m_texture(texture)
		, m_viewDef(viewDefinition)
	{ }

	RGTextureHandle GetTexture() const
	{
		return m_texture;
	}

	rhi::TextureViewDefinition GetViewDefinition() const
	{
		return m_viewDef;
	}

private:

	RGTextureHandle				m_texture;
	rhi::TextureViewDefinition	m_viewDef;

};


class RGBuffer : public RGResource
{
public:

	RGBuffer(const RGResourceDef& resourceDefinition, const rhi::BufferDefinition& definition, const rhi::RHIAllocationInfo& allocationInfo)
		: RGResource(resourceDefinition)
		, m_bufferDef(definition)
		, m_allocationInfo(allocationInfo)
	{ }

	const rhi::BufferDefinition& GetBufferUsage()
	{
		return m_bufferDef;
	}

	const rhi::RHIAllocationInfo& GetAllocationInfo() const
	{
		return m_allocationInfo;
	}

private:

	rhi::BufferDefinition	m_bufferDef;
	rhi::RHIAllocationInfo	m_allocationInfo;
};

using RGBufferHandle = RGResourceHandle<RGBuffer>;

} // spt::rg