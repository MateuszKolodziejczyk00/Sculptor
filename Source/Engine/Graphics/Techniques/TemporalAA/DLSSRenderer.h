#pragma once

#include "SculptorCoreTypes.h"
#include "TemporalAATypes.h"
#include "GraphicsMacros.h"

#include "SculptorDLSS.h"


namespace spt::gfx
{

class GRAPHICS_API DLSSRenderer : public TemporalAARenderer
{
protected:

	using Super = TemporalAARenderer;

public:

	DLSSRenderer();

	// Begin TemporalAARenderer overrides
	virtual Bool Initialize(const TemporalAAInitSettings& initSettings) override;
	virtual math::Vector2f ComputeJitter(Uint64 frameIdx, math::Vector2u resolution) const override;
	virtual Bool PrepareForRendering(const TemporalAAParams& params) override;
	virtual void Render(rg::RenderGraphBuilder& graphBuilder, const TemporalAARenderingParams& renderingParams) override;
	// End TemporalAARenderer overrides

private:

	dlss::SculptorDLSSBackend m_dlssBackend;
};

} // spt::gfx