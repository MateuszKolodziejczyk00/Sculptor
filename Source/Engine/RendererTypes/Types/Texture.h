#pragma once

#include "RendererTypesMacros.h"
#include "RHIBridge/RHITextureImpl.h"
#include "RendererResource.h"
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


class RENDERER_TYPES_API Texture : public RendererResource<rhi::RHITexture>, public lib::SharedFromThis<Texture>
{
protected:

	using ResourceType = RendererResource<rhi::RHITexture>;

public:

	Texture(const RendererResourceName& name, const rhi::TextureDefinition& textureDefinition, const rhi::RHIAllocationInfo& allocationInfo);
	Texture(const RendererResourceName& name, const rhi::RHITexture& rhiTexture);

	lib::SharedPtr<TextureView>		CreateView(const RendererResourceName& name, const rhi::TextureViewDefinition& viewDefinition) const;
};


class RENDERER_TYPES_API TextureView : public RendererResource<rhi::RHITextureView>
{
protected:

	using ResourceType = RendererResource<rhi::RHITextureView>;

public:

	TextureView(const RendererResourceName& name, const lib::SharedPtr<const Texture>& texture, const rhi::TextureViewDefinition& viewDefinition);

	lib::SharedPtr<const Texture>	GetTexture() const;

private:

	lib::WeakPtr<const Texture>		m_texture;
};

}
