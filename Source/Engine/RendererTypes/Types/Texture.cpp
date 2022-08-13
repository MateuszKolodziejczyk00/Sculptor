#include "Texture.h"


namespace spt::renderer
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// Texture =======================================================================================

Texture::Texture(const RendererResourceName& name, const rhi::TextureDefinition& textureDefinition, const rhi::RHIAllocationInfo& allocationInfo)
{
	SPT_PROFILE_FUNCTION();

	GetRHI().InitializeRHI(textureDefinition, allocationInfo);
	GetRHI().SetName(name.Get());
}

Texture::Texture(const RendererResourceName& name, const rhi::RHITexture& rhiTexture)
{
	SPT_CHECK(rhiTexture.IsValid());

	GetRHI() = rhiTexture;
	GetRHI().SetName(name.Get());
}

lib::SharedPtr<TextureView> Texture::CreateView(const RendererResourceName& name, const rhi::TextureViewDefinition& viewDefinition) const
{
	SPT_PROFILE_FUNCTION();

	return std::make_shared<TextureView>(name, shared_from_this(), viewDefinition);
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// Texture =======================================================================================

TextureView::TextureView(const RendererResourceName& name, const lib::SharedPtr<const Texture>& texture, const rhi::TextureViewDefinition& viewDefinition)
	: m_texture(texture)
{
	SPT_PROFILE_FUNCTION();

	SPT_CHECK(!!texture);

	const rhi::RHITexture& rhiTexture = texture->GetRHI();

	GetRHI().InitializeRHI(rhiTexture, viewDefinition);
	GetRHI().SetName(name.Get());
}

lib::SharedPtr<const Texture> TextureView::GetTexture() const
{
	return m_texture.lock();
}

}
