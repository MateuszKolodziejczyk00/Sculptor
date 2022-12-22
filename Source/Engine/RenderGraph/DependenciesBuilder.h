#pragma once

#include "RenderGraphMacros.h"
#include "SculptorCoreTypes.h"
#include "RGResources/RGResourceHandles.h"
#include "RGResources/RGResources.h"


namespace spt::rg
{

class RenderGraphBuilder;


struct RGTextureAccessDef
{
	RGTextureViewHandle	textureView;
	ERGTextureAccess			access;
};


struct RGBufferAccessDef
{
	RGBufferHandle	resource;
	ERGTextureAccess		access;
};


struct RGDependeciesContainer
{
	lib::DynamicArray<RGTextureAccessDef> textureAccesses;
	lib::DynamicArray<RGBufferAccessDef>  bufferAccesses;
};


class RENDER_GRAPH_API RGDependenciesBuilder
{
public:

	explicit RGDependenciesBuilder(RenderGraphBuilder& graphBuilder, RGDependeciesContainer& dependecies);

	void AddTextureAccess(RGTextureViewHandle texture, ERGTextureAccess access);
	void AddTextureAccess(const lib::SharedRef<rdr::TextureView>& texture, ERGTextureAccess access);

	void AddBufferAccess(RGBufferHandle buffer, ERGTextureAccess access);

private:

	RenderGraphBuilder& m_graphBuilder;
	RGDependeciesContainer& m_dependeciesRef;
};

} // spt::rg
