#pragma once

#include "RendererTypesMacros.h"
#include "RHIBridge/RHITextureImpl.h"
#include "RendererUtils.h"


namespace spt::rhi
{
struct RHIAllocationInfo;

struct TextureDefinition;
struct TextureViewDefinition;
}


namespace spt::renderer
{

class TextureView;


class RENDERER_TYPES_API Texture : public lib::SharedFromThis<Texture>
{
public:

	Texture(const RendererResourceName& name, const rhi::TextureDefinition& textureDefinition, const rhi::RHIAllocationInfo& allocationInfo);
	Texture(const RendererResourceName& name, const rhi::RHITexture& rhiTexture);
	~Texture();

	rhi::RHITexture&				GetRHI();
	const rhi::RHITexture&			GetRHI() const;

	lib::SharedPtr<TextureView>		CreateView(const RendererResourceName& name, const rhi::TextureViewDefinition& viewDefinition) const;

private:

	rhi::RHITexture					m_rhiTexture;
};


class RENDERER_TYPES_API TextureView
{
public:

	TextureView(const RendererResourceName& name, const lib::SharedPtr<const Texture>& texture, const rhi::TextureViewDefinition& viewDefinition);
	~TextureView();

	rhi::RHITextureView&			GetRHI();
	const rhi::RHITextureView&		GetRHI() const;

	lib::SharedPtr<const Texture>	GetTexture() const;

private:

	lib::WeakPtr<const Texture>		m_texture;

	rhi::RHITextureView				m_rhiView;
};

}
