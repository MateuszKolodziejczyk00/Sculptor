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
			const rhi::ETextureUsage requiredUsageFlags = lib::Flags(rhi::ETextureUsage::ColorRT, rhi::ETextureUsage::SampledTexture);

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

GPUGBuffer GBuffer::GetGPUGBuffer(rg::RGTextureViewHandle depth) const
{
	GPUGBuffer gpuGBuffer;
	gpuGBuffer.resolution = m_textures[0]->GetResolution2D();
	gpuGBuffer.pixelSize  = math::Vector2f::Ones().cwiseQuotient(gpuGBuffer.resolution.cast<Real32>());
	gpuGBuffer.depth      = depth;
	gpuGBuffer.gBuffer0   = m_textures[0];
	gpuGBuffer.gBuffer1   = m_textures[1];
	gpuGBuffer.gBuffer2   = m_textures[2];
	gpuGBuffer.gBuffer3   = m_textures[3];
	gpuGBuffer.gBuffer4   = m_textures[4];

	return gpuGBuffer;
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

void TextureWithHistory::Update(rhi::RHIResolution resolution)
{
	m_definition.resolution = resolution;

	std::swap(m_current, m_history);

	if (!m_current || m_current->GetResolution() != resolution.AsVector())
	{
		m_current = rdr::ResourcesManager::CreateTextureView(m_name, m_definition, rhi::EMemoryUsage::GPUOnly);
	}

	if (m_history && m_history->GetResolution() != resolution.AsVector())
	{
		m_history.reset();
	}
}

} // spt::rsc
