#pragma once

#include "SculptorCoreTypes.h"
#include "TemporalAATypes.h"
#include "GraphicsMacros.h"


namespace spt::rdr
{
class TextureView;
} // spt::rdr


namespace spt::gfx
{

class GRAPHICS_API StandardTAARenderer : public TemporalAARenderer
{
protected:

	using Super = TemporalAARenderer;

public:

	StandardTAARenderer();

	// Begin TemporalAARenderer overrides
	virtual Bool Initialize(const TemporalAAInitSettings& initSettings) override;
	virtual math::Vector2f ComputeJitter(Uint64 frameIdx, math::Vector2u resolution) const override;
	virtual Bool PrepareForRendering(const TemporalAAParams& params) override;
	virtual void Render(rg::RenderGraphBuilder& graphBuilder, const TemporalAARenderingParams& renderingParams) override;
	// End TemporalAARenderer overrides

private:

	lib::SharedPtr<rdr::TextureView> m_historyTextureView;
	Bool m_hasValidHistory = false;
};

} // spt::gfx
