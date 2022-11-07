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


enum class ERGResourceFlags : Flags32
{
	None = 0,
	Persistent	= BIT(0),

	Default = None
};


using RGResourceID = SizeType;


struct RGResourceDef
{
	RGResourceDef()
		: id(idxNone<RGResourceID>)
		, flags(ERGResourceFlags::None)
	{ }

	RGResourceID			id;
	RenderGraphDebugName	name;
	ERGResourceFlags		flags;
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
		return m_name.Get();
	}

	ERGResourceFlags GetFlags() const
	{
		return m_flags;
	}

private:

	RGResourceID			m_id;
	RenderGraphDebugName	m_name;
	ERGResourceFlags		m_flags;
};


template<typename TResourceType>
class RGResourceHandle
{
public:

	RGResourceHandle()
		: m_id(idxNone<RGResourceID>)
	{ }

	RGResourceHandle(const RGResource& resource)
		: m_id(resource.GetID())
		, m_name(resource.GetName())
	{ }

	RGResourceID GetID() const
	{
		return m_id;
	}

	const lib::HashedString& GetName() const
	{
		return m_name;
	}

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