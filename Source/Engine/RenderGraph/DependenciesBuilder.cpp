#include "DependenciesBuilder.h"
#include "RenderGraphBuilder.h"
#include "Types/Texture.h"


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

void RGDependenciesBuilder::AddTextureAccess(rdr::ResourceDescriptorIdx textureDescriptor, ERGTextureAccess access, RGDependencyStages dependencyStages /*= RGDependencyStages()*/)
{
	if (textureDescriptor != rdr::invalidResourceDescriptorIdx)
	{
		const rdr::DescriptorManager& descriptorManager = rdr::Renderer::GetDescriptorManager();

		// try getting RG texture view that is associated with the descriptor
		RGTextureViewHandle rgTextureView = reinterpret_cast<RGTextureView*>(descriptorManager.GetCustomDescriptorInfo(textureDescriptor));

		// if the RG resource is not associated with the descriptor, try finding external texture view
		if (!rgTextureView.IsValid())
		{
			rdr::TextureView* boundTexture = descriptorManager.GetTextureView(textureDescriptor);
			SPT_CHECK(!!boundTexture);

			const lib::SharedPtr<rdr::TextureView> textureView = boundTexture->AsShared();
			SPT_CHECK(!!textureView);

			rgTextureView = m_graphBuilder.AcquireExternalTextureView(textureView);
		}

		SPT_CHECK(rgTextureView.IsValid());

		AddTextureAccess(rgTextureView, access, dependencyStages);
	}
}

void RGDependenciesBuilder::AddTextureAccessIfAcquired(const lib::SharedRef<rdr::TextureView>& texture, ERGTextureAccess access, RGDependencyStages dependencyStages /*= RGDependencyStages()*/)
{
	if (m_graphBuilder.IsTextureAcquired(texture->GetTexture()))
	{
		const RGTextureViewHandle rgTextureView = m_graphBuilder.AcquireExternalTextureView(texture.ToSharedPtr());
		AddTextureAccess(rgTextureView, access, dependencyStages);
	}
}

void RGDependenciesBuilder::AddBufferAccess(RGBufferViewHandle buffer, const RGBufferAccessInfo& access, RGDependencyStages dependencyStages /*= RGDependencyStages()*/)
{
	const rhi::EPipelineStage accessStages = dependencyStages.shouldUseDefaultStages ? m_defaultStages : dependencyStages.pipelineStages;

	RGBufferAccessDef accessDef{ buffer, access.access, accessStages };
#if DEBUG_RENDER_GRAPH
	accessDef.structTypeName = access.structTypeName;
	accessDef.elementsNum    = access.elementsNum;
#endif // DEBUG_RENDER_GRAPH

	m_dependeciesRef.bufferAccesses.emplace_back(std::move(accessDef));
}

void RGDependenciesBuilder::AddBufferAccess(const rdr::BufferView& buffer, const RGBufferAccessInfo& access, RGDependencyStages dependencyStages /*= RGDependencyStages()*/)
{
	const RGBufferViewHandle rgBufferView = m_graphBuilder.AcquireExternalBufferView(buffer);
	AddBufferAccess(rgBufferView, access, dependencyStages.pipelineStages);
}

} // spt::rg