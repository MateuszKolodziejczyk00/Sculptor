#include "SceneRenderingTypes.h"
#include "RenderGraphBuilder.h"


namespace spt::rsc
{

GBuffer::GBuffer()
{
}

void GBuffer::Create(rg::RenderGraphBuilder& graphBuilder, math::Vector2u resolution)
{
	SPT_PROFILER_FUNCTION();

	for (Uint32 textureIdx = 0u; textureIdx < texturesNum; ++textureIdx)
	{
		m_textures[textureIdx] = graphBuilder.CreateTextureView(RG_DEBUG_NAME_FORMATTED("G-Buffer {}", textureIdx), rg::TextureDef(resolution, formats[textureIdx]));
	}
}

} // spt::rsc
