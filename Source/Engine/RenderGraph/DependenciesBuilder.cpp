#include "DependenciesBuilder.h"
#include "RenderGraphBuilder.h"

namespace spt::rg
{

RGDependenciesBuilder::RGDependenciesBuilder(RenderGraphBuilder& graphBuilder, RGDependeciesContainer& dependecies, rhi::EPipelineStage defaultStages /*= rhi::EPipelineStage::None*/) 
	: m_graphBuilder(graphBuilder)
	, m_dependeciesRef(dependecies)
	, m_defaultStages(defaultStages)
{ }

void RGDependenciesBuilder::AddTextureAccess(RGTextureViewHandle texture, ERGTextureAccess access, RGDependencyStages dependencyStages /*= RGDependencyStages()*/)
{
	const rhi::EPipelineStage accessStages = dependencyStages.shouldUseDefaultStages ? m_defaultStages : dependencyStages.pipelineStages;
	m_dependeciesRef.textureAccesses.emplace_back(RGTextureAccessDef{ texture, access, accessStages });
}

void RGDependenciesBuilder::AddTextureAccess(const lib::SharedRef<rdr::TextureView>& texture, ERGTextureAccess access, RGDependencyStages dependencyStages /*= RGDependencyStages()*/)
{
	const RGTextureViewHandle rgTextureView = m_graphBuilder.AcquireExternalTextureView(texture.ToSharedPtr());
	AddTextureAccess(rgTextureView, access, dependencyStages.pipelineStages);
}

void RGDependenciesBuilder::AddTextureAccessIfAcquired(const lib::SharedRef<rdr::TextureView>& texture, ERGTextureAccess access, RGDependencyStages dependencyStages /*= RGDependencyStages()*/)
{
	if (m_graphBuilder.IsTextureAcquired(texture->GetTexture()))
	{
		const RGTextureViewHandle rgTextureView = m_graphBuilder.AcquireExternalTextureView(texture.ToSharedPtr());
		AddTextureAccess(rgTextureView, access, dependencyStages);
	}
}

void RGDependenciesBuilder::AddBufferAccess(RGBufferViewHandle buffer, ERGBufferAccess access, RGDependencyStages dependencyStages /*= RGDependencyStages()*/)
{
	const rhi::EPipelineStage accessStages = dependencyStages.shouldUseDefaultStages ? m_defaultStages : dependencyStages.pipelineStages;
	m_dependeciesRef.bufferAccesses.emplace_back(RGBufferAccessDef{ buffer, access, accessStages });
}

void RGDependenciesBuilder::AddBufferAccess(const rdr::BufferView& buffer, ERGBufferAccess access, RGDependencyStages dependencyStages /*= RGDependencyStages()*/)
{
	const RGBufferViewHandle rgBufferView = m_graphBuilder.AcquireExternalBufferView(buffer);
	AddBufferAccess(rgBufferView, access, dependencyStages.pipelineStages);
}

} // spt::rg