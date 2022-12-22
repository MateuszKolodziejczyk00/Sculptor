#include "DependenciesBuilder.h"
#include "RenderGraphBuilder.h"

namespace spt::rg
{

RGDependenciesBuilder::RGDependenciesBuilder(RenderGraphBuilder& graphBuilder, RGDependeciesContainer& dependecies) 
	: m_graphBuilder(graphBuilder)
	, m_dependeciesRef(dependecies)
{ }

void RGDependenciesBuilder::AddTextureAccess(RGTextureViewHandle texture, ERGTextureAccess access, rhi::EShaderStageFlags shaderStages /*= rhi::EShaderStageFlags::None*/)
{
	m_dependeciesRef.textureAccesses.emplace_back(RGTextureAccessDef{ texture, access, shaderStages });
}

void RGDependenciesBuilder::AddTextureAccess(const lib::SharedRef<rdr::TextureView>& texture, ERGTextureAccess access, rhi::EShaderStageFlags shaderStages /*= rhi::EShaderStageFlags::None*/)
{
	const RGTextureViewHandle rgTextureView = m_graphBuilder.AcquireExternalTextureView(texture.ToSharedPtr());
	AddTextureAccess(rgTextureView, access, shaderStages);
}

void RGDependenciesBuilder::AddBufferAccess(RGBufferViewHandle buffer, ERGBufferAccess access, rhi::EShaderStageFlags shaderStages /*= rhi::EShaderStageFlags::None*/)
{
	m_dependeciesRef.bufferAccesses.emplace_back(RGBufferAccessDef{ buffer, access, shaderStages });
}

void RGDependenciesBuilder::AddBufferAccess(const lib::SharedRef<rdr::BufferView>& buffer, ERGBufferAccess access, rhi::EShaderStageFlags shaderStages /*= rhi::EShaderStageFlags::None*/)
{
	const RGBufferViewHandle rgBufferView = m_graphBuilder.AcquireExternalBufferView(buffer.ToSharedPtr());
	AddBufferAccess(rgBufferView, access, shaderStages);
}

} // spt::rg