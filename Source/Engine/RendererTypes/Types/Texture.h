#pragma once

#include "RendererTypesMacros.h"
#include "RHITextureImpl.h"
#include "RendererUtils.h"


namespace spt::rhi
{
struct RHIAllocationInfo;

struct TextureDefinition;
struct TextureViewDefinition;
}


namespace spt::renderer
{

class RENDERER_TYPES_API Texture
{
public:

	Texture(const RendererResourceName& name, const rhi::TextureDefinition& textureDefinition, const rhi::RHIAllocationInfo& allocationInfo);
	~Texture();

	rhi::RHITexture&				GetRHI();
	const rhi::RHITexture&			GetRHI() const;

private:

	rhi::RHITexture					m_rhiTexture;
};

}
