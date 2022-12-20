#pragma once

#include "RendererCoreMacros.h"
#include "RHIBridge/RHITextureImpl.h"
#include "RHICore/RHIAllocationTypes.h"
#include "RendererResource.h"
#include "RendererUtils.h"


namespace spt::rhi
{
struct TextureViewDefinition;
}


namespace spt::rdr
{

class TextureView;


class RENDERER_CORE_API Texture : public RendererResource<rhi::RHITexture>, public lib::SharedFromThis<Texture>
{
protected:

	using ResourceType = RendererResource<rhi::RHITexture>;

public:

	Texture(const RendererResourceName& name, const rhi::TextureDefinition& textureDefinition, const rhi::RHIAllocationInfo& allocationInfo);
	Texture(const RendererResourceName& name, const rhi::RHITexture& rhiTexture);

	void Rename(const RendererResourceName& name);

	const rhi::TextureDefinition&	GetDefinition() const;
	const math::Vector3u&			GetResolution() const;
	math::Vector2u					GetResolution2D() const;

	lib::SharedRef<TextureView>		CreateView(const RendererResourceName& name, const rhi::TextureViewDefinition& viewDefinition) const;
};


class RENDERER_CORE_API TextureView : public RendererResource<rhi::RHITextureView>
{
protected:

	using ResourceType = RendererResource<rhi::RHITextureView>;

public:

	TextureView(const RendererResourceName& name, const lib::SharedPtr<Texture>& texture, const rhi::TextureViewDefinition& viewDefinition);

	lib::SharedPtr<Texture>	GetTexture() const;

private:

	lib::WeakPtr<Texture>		m_texture;
};

} // spt::rdr
