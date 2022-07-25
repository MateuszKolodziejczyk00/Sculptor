#include "Texture.h"
#include "CurrentFrameContext.h"


namespace spt::renderer
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// Texture =======================================================================================

Texture::Texture(const RendererResourceName& name, const rhi::TextureDefinition& textureDefinition, const rhi::RHIAllocationInfo& allocationInfo)
{
	SPT_PROFILE_FUNCTION();

	m_rhiTexture.InitializeRHI(textureDefinition, allocationInfo);
	m_rhiTexture.SetName(name.Get());
}

Texture::Texture(const RendererResourceName& name, const rhi::RHITexture& rhiTexture)
{
	SPT_CHECK(rhiTexture.IsValid());

	m_rhiTexture = rhiTexture;
	m_rhiTexture.SetName(name.Get());
}

Texture::~Texture()
{
	SPT_PROFILE_FUNCTION();

	SPT_CHECK(m_rhiTexture.IsValid());

	CurrentFrameContext::GetCurrentFrameCleanupDelegate().AddLambda(
		[resource = m_rhiTexture]() mutable
		{
			resource.ReleaseRHI();
		});
}

rhi::RHITexture& Texture::GetRHI()
{
	return m_rhiTexture;
}

const rhi::RHITexture& Texture::GetRHI() const
{
	return m_rhiTexture;
}

lib::SharedPtr<TextureView> Texture::CreateView(const rhi::TextureViewDefinition& viewDefinition) const
{
	SPT_PROFILE_FUNCTION();

	return std::make_shared<TextureView>(shared_from_this(), viewDefinition);
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// Texture =======================================================================================

TextureView::TextureView(const lib::SharedPtr<const Texture>& texture, const rhi::TextureViewDefinition& viewDefinition)
	: m_texture(texture)
{
	SPT_PROFILE_FUNCTION();

	SPT_CHECK(!!texture);

	const rhi::RHITexture& rhiTexture = texture->GetRHI();

	m_rhiView.InitializeRHI(rhiTexture, viewDefinition);
}

TextureView::~TextureView()
{
	SPT_PROFILE_FUNCTION();

	// View must be destroyed before texture
	SPT_CHECK(!m_texture.expired());

	SPT_CHECK(m_rhiView.IsValid());

	CurrentFrameContext::GetCurrentFrameCleanupDelegate().AddLambda(
		[resource = m_rhiView]() mutable
		{
			resource.ReleaseRHI();
		});
}

rhi::RHITextureView& TextureView::GetRHI()
{
	return m_rhiView;
}

const rhi::RHITextureView& TextureView::GetRHI() const
{
	return m_rhiView;
}

lib::SharedPtr<const Texture> TextureView::GetTexture() const
{
	return m_texture.lock();
}

}
