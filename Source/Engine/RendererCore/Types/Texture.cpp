#include "Texture.h"


namespace spt::rdr
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// Texture =======================================================================================

Texture::Texture(const RendererResourceName& name, const rhi::TextureDefinition& textureDefinition, const rhi::RHIAllocationInfo& allocationInfo)
{
	SPT_PROFILER_FUNCTION();

	GetRHI().InitializeRHI(textureDefinition, allocationInfo);
	GetRHI().SetName(name.Get());
}

Texture::Texture(const RendererResourceName& name, const rhi::RHITexture& rhiTexture)
{
	SPT_CHECK(rhiTexture.IsValid());

	GetRHI() = rhiTexture;
	GetRHI().SetName(name.Get());
}

void Texture::Rename(const RendererResourceName& name)
{
	GetRHI().SetName(name.Get());
}

const rhi::TextureDefinition& Texture::GetDefinition() const
{
	return GetRHI().GetDefinition();
}

const math::Vector3u& Texture::GetResolution() const
{
	return GetDefinition().resolution;
}

math::Vector2u Texture::GetResolution2D() const
{
	return GetResolution().head<2>();
}

lib::SharedRef<TextureView> Texture::CreateView(const RendererResourceName& name, const rhi::TextureViewDefinition& viewDefinition) const
{
	SPT_PROFILER_FUNCTION();

	return lib::Ref(std::make_shared<TextureView>(name, lib::Ref(const_cast<Texture*>(this)->shared_from_this()), viewDefinition));
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// Texture =======================================================================================

TextureView::TextureView(const RendererResourceName& name, const lib::SharedRef<Texture>& texture, const rhi::TextureViewDefinition& viewDefinition)
	: m_texture(texture)
{
	SPT_PROFILER_FUNCTION();

	const rhi::RHITexture& rhiTexture = texture->GetRHI();

	GetRHI().InitializeRHI(rhiTexture, viewDefinition);
	GetRHI().SetName(name.Get());
}

const lib::SharedRef<Texture>& TextureView::GetTexture() const
{
	return m_texture;
}

} // spt::rdr
