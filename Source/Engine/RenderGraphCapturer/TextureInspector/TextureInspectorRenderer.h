#pragma once

#include "SculptorCoreTypes.h"
#include "TextureInspectorTypes.h"
#include "Types/Texture.h"
#include "RenderGraphResourcesPool.h"


namespace spt::rg::capture
{

class TextureInspectorRenderer : std::enable_shared_from_this<TextureInspectorRenderer>
{
public:

	explicit TextureInspectorRenderer();

	void SetParameters(const TextureInspectorFilterParams& parameters);

	void Render(const lib::SharedRef<rdr::TextureView>& inputTexture, const lib::SharedRef<rdr::TextureView>& outputTexture, const lib::SharedRef<TextureInspectorReadback>& readback);

private:

	TextureInspectorFilterParams m_parameters;

	rg::RenderGraphResourcesPool m_resourcesPool;
};

} // spt::rg::capture
