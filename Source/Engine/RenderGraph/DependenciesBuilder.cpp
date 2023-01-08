#include "DependenciesBuilder.h"
#include "RenderGraphBuilder.h"

namespace spt::rg
{

RGDependenciesBuilder::RGDependenciesBuilder(RenderGraphBuilder& graphBuilder, RGDependeciesContainer& dependecies) 
	: m_graphBuilder(graphBuilder)
	, m_dependeciesRef(dependecies)
{ }

void RGDependenciesBuilder::AddTextureAccess(RGTextureViewHandle texture, ERGTextureAccess access, RGDependencyStages dependencyStages /*= RGDependencyStages()*/)
{
	m_dependeciesRef.textureAccesses.emplace_back(RGTextureAccessDef{ texture, access, dependencyStages.pipelineStages });
}

void RGDependenciesBuilder::AddTextureAccess(const lib::SharedRef<rdr::TextureView>& texture, ERGTextureAccess access, RGDependencyStages dependencyStages /*= RGDependencyStages()*/)
{
	const RGTextureViewHandle rgTextureView = m_graphBuilder.AcquireExternalTextureView(texture.ToSharedPtr());
	AddTextureAccess(rgTextureView, access, dependencyStages.pipelineStages);
}

void RGDependenciesBuilder::AddBufferAccess(RGBufferViewHandle buffer, ERGBufferAccess access, RGDependencyStages dependencyStages /*= RGDependencyStages()*/)
{
	m_dependeciesRef.bufferAccesses.emplace_back(RGBufferAccessDef{ buffer, access, dependencyStages.pipelineStages });
}

void RGDependenciesBuilder::AddBufferAccess(const lib::SharedRef<rdr::BufferView>& buffer, ERGBufferAccess access, RGDependencyStages dependencyStages /*= RGDependencyStages()*/)
{
	const RGBufferViewHandle rgBufferView = m_graphBuilder.AcquireExternalBufferView(buffer.ToSharedPtr());
	AddBufferAccess(rgBufferView, access, dependencyStages.pipelineStages);
}

} // spt::rg