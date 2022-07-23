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

Texture::~Texture()
{
	SPT_PROFILE_FUNCTION();

	SPT_CHECK(m_rhiTexture.IsValid());

	CurrentFrameContext::SubmitDeferredRelease(
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

}
