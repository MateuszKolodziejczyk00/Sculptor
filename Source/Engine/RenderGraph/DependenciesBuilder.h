#pragma once

#include "RGResources/RenderGraphResource.h"


namespace spt::rg
{

struct RGTextureAccessDef
{
	RGTextureView	textureView;
	ERGAccess		access;
};


struct RGBufferAccessDef
{
	RGBufferHandle	resource;
	ERGAccess		access;
};


class RGDependenciesBuilder
{
public:

	RGDependenciesBuilder() = default;

	void AddTextureAccess(RGTextureView texture, ERGAccess access)
	{
		m_textureAccesses.emplace_back(RGTextureAccessDef{ texture, access });
	}

	void AddBufferAccess(RGBufferHandle buffer, ERGAccess access)
	{
		m_bufferAccesses.emplace_back(RGBufferAccessDef{ buffer, access });
	}

	const lib::DynamicArray<RGTextureAccessDef>& GetTextureAccesses() const
	{
		return m_textureAccesses;
	}

	const lib::DynamicArray<RGBufferAccessDef>& GetBufferAccesses() const
	{
		return m_bufferAccesses;
	}

private:

	lib::DynamicArray<RGTextureAccessDef>  m_textureAccesses;
	lib::DynamicArray<RGBufferAccessDef>  m_bufferAccesses;
};

} // spt::rg
