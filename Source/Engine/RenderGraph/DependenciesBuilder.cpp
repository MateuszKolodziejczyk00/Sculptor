#include "DependenciesBuilder.h"
#include "RenderGraphBuilder.h"

namespace spt::rg
{

RGDependenciesBuilder::RGDependenciesBuilder(RenderGraphBuilder& graphBuilder, RGDependeciesContainer& dependecies) 
	: m_graphBuilder(graphBuilder)
	, m_dependeciesRef(dependecies)
{ }

void RGDependenciesBuilder::AddTextureAccess(RGTextureViewHandle texture, ERGAccess access)
{
	m_dependeciesRef.textureAccesses.emplace_back(RGTextureAccessDef{ texture, access });
}

void RGDependenciesBuilder::AddTextureAccess(const lib::SharedRef<rdr::TextureView>& texture, ERGAccess access)
{
	const RGTextureViewHandle rgTextureView = m_graphBuilder.AcquireExternalTextureView(texture.ToSharedPtr());
	AddTextureAccess(rgTextureView, access);
}

void RGDependenciesBuilder::AddBufferAccess(RGBufferHandle buffer, ERGAccess access)
{
	m_dependeciesRef.bufferAccesses.emplace_back(RGBufferAccessDef{ buffer, access });
}

} // spt::rg