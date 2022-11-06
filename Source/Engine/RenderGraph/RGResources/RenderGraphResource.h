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


/**
 * TODO
 * Resource - id only
 * Resource View
 * 
 */

using RGResourceID = SizeType;


class RGResource abstract
{
public:

	RGResource(RGResourceID id, const lib::HashedString& name, ERGResourceFlags flags)
		: m_id(id)
		, m_name(name)
		, m_flags(flags)
	{ }

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


class RGTexture : public RGResource
{
public:

	RGTexture(RGResourceID id, const lib::HashedString& name, ERGResourceFlags flags, const rhi::TextureDefinition& textureDefinition, const rhi::RHIAllocationInfo& allocationInfo)
		: RGResource(id, name, flags)
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


class RGBuffer : public RGResource
{
public:

private:

	Uint64 size;
	rhi::EBufferUsage bufferUsage;
	rhi::RHIAllocationInfo& allocationInfo;
};


template<typename TResourceType>
class RGResourceHandle
{
public:

private:

	RGResourceID			m_id;
	RenderGraphDebugName	m_name;
};


using RHTextureHandle	= RGResourceHandle<RGTexture>;
using RHBufferHandle	= RGResourceHandle<RGBuffer>;

} // spt::rg