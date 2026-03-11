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

	static Bool InitializeDLSS();

	DLSSRenderer();

	// Begin TemporalAARenderer overrides
	Bool Initialize(const TemporalAAInitSettings& initSettings);
	math::Vector2f ComputeJitter(Uint64 frameIdx, math::Vector2u renderingResolution, math::Vector2u outputResolution) const;
	Bool PrepareForRendering(const TemporalAAParams& params);
	void Render(rg::RenderGraphBuilder& graphBuilder, const TemporalAARenderingParams& renderingParams);
	// End TemporalAARenderer overrides

private:

	dlss::SculptorDLSSBackend m_dlssBackend;
};

} // spt::gfx
