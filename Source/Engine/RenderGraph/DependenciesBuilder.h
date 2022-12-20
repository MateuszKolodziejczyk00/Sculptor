#pragma once

#include "RGResources/RGResourceHandles.h"
#include "RGResources/RGResources.h"


namespace spt::rg
{

struct RGTextureAccessDef
{
	RGTextureViewHandle	textureView;
	ERGAccess			access;
};


struct RGBufferAccessDef
{
	RGBufferHandle	resource;
	ERGAccess		access;
};


struct RGDependeciesContainer
{
	lib::DynamicArray<RGTextureAccessDef> textureAccesses;
	lib::DynamicArray<RGBufferAccessDef>  bufferAccesses;
};


class RGDependenciesBuilder
{
public:

	RGDependenciesBuilder(RGDependeciesContainer& dependecies)
		: m_dependeciesRef(dependecies)
	{ }

	void AddTextureAccess(RGTextureViewHandle texture, ERGAccess access)
	{
		m_dependeciesRef.textureAccesses.emplace_back(RGTextureAccessDef{ texture, access });
	}

	void AddBufferAccess(RGBufferHandle buffer, ERGAccess access)
	{
		m_dependeciesRef.bufferAccesses.emplace_back(RGBufferAccessDef{ buffer, access });
	}

private:

	RGDependeciesContainer& m_dependeciesRef;
};

} // spt::rg
