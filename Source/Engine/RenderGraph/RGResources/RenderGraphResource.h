#pragma once

#include "SculptorCoreTypes.h"
#include "RenderGraphTypes.h"
#include "RHICore/RHITextureTypes.h"
#include "RHICore/RHIAllocationTypes.h"
#include "RHICore/RHIBufferTypes.h"


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


enum class ERGResourceFlags
{
	None = 0,

	Default = None
};


using RGResourceID = SizeType;


struct RGResourceDef
{
	RGResourceDef()
		: m_id(idxNone<RGResourceID>)
		, m_flags(ERGResourceFlags::None)
	{ }

	RGResourceID		id;
	lib::HashedString	name;
	ERGResourceFlags	flags;
};


class RGResource abstract
{
public:

	explicit RGResource(const RGResourceDef& definition)
		: m_id(definition.id)
		, m_name(definition.name)
		, m_flags(definition.flags)
	{
		SPT_CHECK(m_id != idxNone<RGResourceID>);
	}

	Bool IsValid() const
	{
		return m_id != idxNone<RGResourceID>;
	}

	RGResourceID GetID() const
	{
		return m_id;
	}

	const lib::HashedString& GetName() const
	{
		return m_name;
	}

	ERGResourceFlags GetFlags() const
	{
		return m_flags;
	}

private:

	RGResourceID		m_id;
	lib::HashedString	m_name;
	ERGResourceFlags	m_flags;
};


template<typename TResourceType>
class RGResourceHandle
{
public:

private:

	RGResourceID			m_id;
	RenderGraphDebugName	m_name;
};


class RGTexture : public RGResource
{
public:

	RGTexture(const RGResourceDef& resourceDefinition, const rhi::TextureDefinition& textureDefinition, const rhi::RHIAllocationInfo& allocationInfo)
		: RGResource(resourceDefinition)
		, m_textureDefinition(textureDefinition)
		, m_allocationInfo(allocationInfo)
	{ }
	
	const rhi::TextureDefinition& GetTextureDefinition() const
	{
		return m_textureDefinition;
	}

	const rhi::RHIAllocationInfo& GetAllocationInfo() const
	{
		return m_allocationInfo;
	}

private:

	rhi::TextureDefinition m_textureDefinition;
	rhi::RHIAllocationInfo m_allocationInfo;
};

using RGTextureHandle = RGResourceHandle<RGTexture>;


class RGTextureView : public RGResource
{
public:

	RGTextureView(const RGResourceDef& resourceDefinition, RGTextureHandle texture, const rhi::TextureViewDefinition& viewDefinition)
		: RGResource(resourceDefinition)
		, m_texture(texture)
		, viewDefinition(m_viewDef)
	{ }

	RGTextureHandle GetTextureView() const
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

using RGTextureViewHandle = RGResourceHandle<RGTextureView>;


class RGBuffer : public RGResource
{
public:

	RGBuffer(const RGResourceDef& resourceDefinition, RGTextureHandle texture, const rhi::TextureViewDefinition& viewDefinition)
		: RGResource(resourceDefinition)
	{ }

	Uint64 GetSize() const
	{
		return m_size;
	}

	rhi::EBufferUsage GetBufferUsage()
	{
		return m_bufferUsage;
	}

	const rhi::RHIAllocationInfo& GetAllocationInfo() const
	{
		return m_allocationInfo;
	}

private:

	Uint64					m_size;
	rhi::EBufferUsage		m_bufferUsage;
	rhi::RHIAllocationInfo	m_allocationInfo;
};

using RGBufferHandle = RGResourceHandle<RGBuffer>;

} // spt::rg