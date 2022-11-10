#pragma once

#include "SculptorCoreTypes.h"
#include "RenderGraphTypes.h"
#include "RHICore/RHITextureTypes.h"
#include "RHICore/RHIAllocationTypes.h"
#include "RHICore/RHIBufferTypes.h"
#include "Types/Texture.h"
#include "Allocator/RenderGraphAllocator.h"


namespace spt::rg
{

class ERGAccess
{
	
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


template<typename TResourceType>
class RGResourceHandle
{
public:

	RGResourceHandle()
		: m_resource(nullptr)
	{ }

	RGResourceHandle(TResourceType* resource)
		: m_resource(resource)
	{ }

	Bool IsValid() const
	{
		return !!m_resource;
	}

	TResourceType* Get() const
	{
		return m_resource;
	}

	TResourceType* operator->() const
	{
		return m_resource;
	}

	void Reset()
	{
		m_resource = nullptr;
	}

private:

	TResourceType* m_resource;
};


class RGTexture : public RGResource
{
public:

	RGTexture(const RGResourceDef& resourceDefinition, const rhi::TextureDefinition& textureDefinition, const rhi::RHIAllocationInfo& allocationInfo)
		: RGResource(resourceDefinition)
		, m_textureDefinition(textureDefinition)
		, m_allocationInfo(allocationInfo)
		, m_extractionDest(nullptr)
	{ }

	RGTexture(const RGResourceDef& resourceDefinition, lib::SharedPtr<rdr::Texture> texture)
		: RGResource(resourceDefinition)
		, m_textureDefinition(texture->GetRHI().GetDefinition())
		, m_allocationInfo(texture->GetRHI().GetAllocationInfo())
		, m_texture(texture)
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

private:

	rhi::TextureDefinition m_textureDefinition;
	rhi::RHIAllocationInfo m_allocationInfo;

	lib::SharedPtr<rdr::Texture> m_texture;

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