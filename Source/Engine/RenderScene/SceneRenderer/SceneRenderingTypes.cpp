#include "SceneRenderingTypes.h"
#include "RenderGraphBuilder.h"


namespace spt::rsc
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// GBuffer =======================================================================================

GBuffer::GBuffer()
{
}

void GBuffer::Create(rg::RenderGraphBuilder& graphBuilder, math::Vector2u resolution, const GBufferExternalTextures& externalTextures /*= GBufferExternalTextures()*/)
{
	SPT_PROFILER_FUNCTION();

	for (Uint32 textureIdx = 0u; textureIdx < texturesNum; ++textureIdx)
	{
		lib::SharedPtr<rdr::TextureView>* externalTexture = externalTextures.textures[textureIdx];
		if (externalTexture)
		{
			const rhi::ETextureUsage requiredUsageFlags = lib::Flags(rhi::ETextureUsage::StorageTexture, rhi::ETextureUsage::SampledTexture);

			if (!(*externalTexture) || (*externalTexture)->GetResolution2D() != resolution)
			{
				rhi::TextureDefinition textureDef;
				textureDef.resolution = resolution;
				textureDef.format     = formats[textureIdx];
				textureDef.usage      = requiredUsageFlags;
				*externalTexture = rdr::ResourcesManager::CreateTextureView(RENDERER_RESOURCE_NAME_FORMATTED("G-Buffer {}", textureIdx), textureDef, rhi::EMemoryUsage::GPUOnly);
			}
			else
			{
				const lib::SharedPtr<rdr::Texture>& textureInstance = (*externalTexture)->GetTexture();
				const rhi::TextureDefinition& textureDef = textureInstance->GetDefinition();

				SPT_CHECK_MSG(textureDef.format == formats[textureIdx], "External texture format mismatch");
				SPT_CHECK_MSG(lib::HasAllFlags(textureDef.usage, requiredUsageFlags), "External texture usage flags mismatch");
			}

			m_textures[textureIdx] = graphBuilder.AcquireExternalTextureView(*externalTexture);
		}
		else
		{
			m_textures[textureIdx] = graphBuilder.CreateTextureView(RG_DEBUG_NAME_FORMATTED("G-Buffer {}", textureIdx), rg::TextureDef(resolution, formats[textureIdx]));
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// TextureWithHistory ============================================================================

TextureWithHistory::TextureWithHistory(rdr::RendererResourceName name)
	: m_name(name)
{
}

void TextureWithHistory::SetDefinition(const rhi::TextureDefinition& definition)
{
	m_definition = definition;
	Reset();
}

void TextureWithHistory::Reset()
{
	m_current.reset();
	m_history.reset();
}

void TextureWithHistory::Update(math::Vector2u resolution)
{
	m_definition.resolution = resolution;

	std::swap(m_current, m_history);

	if (!m_current || m_current->GetResolution2D() != resolution)
	{
		m_current = rdr::ResourcesManager::CreateTextureView(m_name, m_definition, rhi::EMemoryUsage::GPUOnly);
	}

	if (m_history && m_history->GetResolution2D() != resolution)
	{
		m_history.reset();
	}
}

} // spt::rsc
