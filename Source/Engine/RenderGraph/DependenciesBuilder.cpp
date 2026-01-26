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
	if (lib::ContainsPred(m_dependeciesRef.textureAccesses, [texture](const RGTextureAccessDef& access) { return access.textureView == texture.Get(); }))
	{
		return;
	}

	const rhi::EPipelineStage accessStages = dependencyStages.shouldUseDefaultStages ? m_defaultStages : dependencyStages.pipelineStages;
	m_dependeciesRef.textureAccesses.EmplaceBack(RGTextureAccessDef{ texture.Get(), access, accessStages });
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

void RGDependenciesBuilder::AddBufferAccess(const lib::SharedPtr<rdr::Buffer>& buffer, const RGBufferAccessInfo& access, RGDependencyStages dependencyStages /* = RGDependencyStages() */)
{
	AddBufferAccess(m_graphBuilder.AcquireExternalBufferView(buffer->GetFullView()), access, dependencyStages);
}

void RGDependenciesBuilder::AddBufferAccess(RGBufferViewHandle buffer, const RGBufferAccessInfo& access, RGDependencyStages dependencyStages /*= RGDependencyStages()*/)
{
	if (lib::ContainsPred(m_dependeciesRef.bufferAccesses, [buffer](const RGBufferAccessDef& access) { return access.resource == buffer.Get(); }))
	{
		return;
	}

	const rhi::EPipelineStage accessStages = dependencyStages.shouldUseDefaultStages ? m_defaultStages : dependencyStages.pipelineStages;

	RGBufferAccessDef accessDef{ buffer.Get(), access.access, accessStages };
#if DEBUG_RENDER_GRAPH
	accessDef.structTypeName = access.structTypeName;
	accessDef.elementsNum    = access.elementsNum;
	accessDef.namedBuffer    = access.namedBuffer;
#endif // DEBUG_RENDER_GRAPH

	m_dependeciesRef.bufferAccesses.EmplaceBack(std::move(accessDef));
}

void RGDependenciesBuilder::AddBufferAccess(const lib::SharedPtr<rdr::BindableBufferView>& buffer, const RGBufferAccessInfo& access, RGDependencyStages dependencyStages /*= RGDependencyStages()*/)
{
	const RGBufferViewHandle rgBufferView = m_graphBuilder.AcquireExternalBufferView(buffer);
	AddBufferAccess(rgBufferView, access, dependencyStages.pipelineStages);
}

void RGDependenciesBuilder::AddBufferAccess(rdr::ResourceDescriptorIdx bufferDescriptor, const RGBufferAccessInfo& access, RGDependencyStages dependencyStages /*= RGDependencyStages()*/)
{
	if (bufferDescriptor != rdr::invalidResourceDescriptorIdx)
	{
		const rdr::DescriptorManager& descriptorManager = rdr::Renderer::GetDescriptorManager();

		// try getting RG texture view that is associated with the descriptor
		RGBufferViewHandle rgBufferView = reinterpret_cast<RGBufferView*>(descriptorManager.GetCustomDescriptorInfo(bufferDescriptor));

		// if the RG resource is not associated with the descriptor, try finding external texture view
		if (!rgBufferView.IsValid())
		{
			rdr::BindableBufferView* boundBufferViewPtr = descriptorManager.GetBufferView(bufferDescriptor);
			SPT_CHECK(!!boundBufferViewPtr);

			const lib::SharedPtr<rdr::BindableBufferView> bufferView = boundBufferViewPtr->AsSharedPtr();
			SPT_CHECK(!!bufferView);

			rgBufferView = m_graphBuilder.AcquireExternalBufferView(bufferView);
		}

		SPT_CHECK(rgBufferView.IsValid());

		AddBufferAccess(rgBufferView, access, dependencyStages);
	}
}

} // spt::rg
